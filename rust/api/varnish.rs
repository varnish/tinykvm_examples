/**
 * Copyright (c) 2025 Varnish Software AS
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
**/
use std::arch::asm;
use std::cell::RefCell;
use std::mem::MaybeUninit;
use core::ffi::CStr;
use core::ffi::c_char;

/** HTTP **/
#[allow(dead_code)]
pub static REQ:  u8 = 0;
#[allow(dead_code)]
pub static RESP: u8 = 1;

#[inline]
#[allow(dead_code)]
pub fn log(field: &str) -> i32
{
	let mut result = 0x7F000;
	unsafe {
		asm!("out 0x0, eax",
			inout("eax") result,
			in("rdi") field.as_ptr(),
			in("rsi") field.len()
		);
	}
	return result
}

#[inline]
#[allow(dead_code)]
pub fn append(hp: u8, field: &str) -> i32
{
	let mut result = 0x10020;
	unsafe {
		asm!("out 0x0, eax",
			inout("eax") result,
			in("rdi") hp as i32,
			in("rsi") field.as_ptr(),
			in("rdx") field.len()
		);
	}
	return result
}

#[inline]
#[allow(dead_code)]
pub fn set_cacheable(cached: bool, ttl: f32, grace: f32, keep: f32)
{
	unsafe {
		asm!("out 0x0, eax",
			in("eax") 0x10005,
			in("rdi") cached as i32,
			in("rsi") (ttl * 1000.0) as i64,
			in("rdx") (grace * 1000.0) as i64,
			in("rcx") (keep * 1000.0) as i64
		);
	}
}

#[inline]
#[allow(dead_code)]
pub fn backend_response(status: u16, ctype: &str, data: &[u8])
{
	unsafe {
		asm!("out 0x0, eax",
			in("eax") 0x10010,
			in("rdi") status,
			in("rsi") ctype.as_ptr(),
			in("rdx") ctype.len(),
			in("rcx") data.as_ptr(),
			in("r8")  data.len(),
			in("r9")  0 // Extra data
		);
	}
}

#[allow(dead_code)]
pub struct ResponseHeader {
	pub data: *const u8,
	pub len: usize,
}
#[allow(dead_code)]
pub struct ExtraResponseData {
	pub headers: *const ResponseHeader,
	pub num_headers: usize,
	pub cached: bool,
	pub ttl: f32,
	pub grace: f32,
	pub keep: f32,
}

#[inline]
#[allow(dead_code)]
pub fn backend_response_full(status: u16, ctype: &str, data: &[u8], extra: &ExtraResponseData)
{
	unsafe {
		asm!("out 0x0, eax",
			in("eax") 0x10010,
			in("rdi") status,
			in("rsi") ctype.as_ptr(),
			in("rdx") ctype.len(),
			in("rcx") data.as_ptr(),
			in("r8")  data.len(),
			in("r9")  extra as *const ExtraResponseData
		);
	}
}

#[inline]
#[allow(dead_code)]
pub fn backend_response_str(status: u16, ctype: &str, data: &str)
{
	backend_response(status, ctype, data.as_bytes());
}

/** Methods **/
pub type GetHandler = fn(url: &str, arg: &str);
fn default_get_handler(_url: &str, _arg: &str) {
	backend_response_str(404, "text/plain", "No such page");
}
pub type PostHandler = fn(url: &str, arg: &str, ctype: &str, data: &mut [u8]);
fn default_post_handler(_url: &str, _arg: &str, _ctype: &str, _data: &mut [u8]) {
	backend_response_str(404, "text/plain", "No such page");
}

thread_local! {
    static ON_GET_HANDLER: RefCell<GetHandler>
        = RefCell::new(default_get_handler);
    static ON_POST_HANDLER: RefCell<PostHandler>
        = RefCell::new(default_post_handler);
}

extern "C"
fn sys_on_get(c_url: *const c_char, c_arg: *const c_char)
{
	let url = unsafe { CStr::from_ptr(c_url).to_str().unwrap() };
	let arg = unsafe { CStr::from_ptr(c_arg).to_str().unwrap() };

	ON_GET_HANDLER.with(|handler| handler.borrow()(url, arg));
	default_get_handler(url, arg);
}
pub fn set_backend_get(cb: GetHandler)
{
	ON_GET_HANDLER.with(|handler| *handler.borrow_mut() = cb);
	unsafe {
		asm!("out 0x0, eax",
			in("eax") 0x10000,
			in("rdi") 1,
			in("rsi") sys_on_get
		);
	}
}

extern "C"
fn sys_on_post(c_url: *mut i8, c_arg: *mut i8, c_ctype: *mut i8, c_data: *mut u8, c_size: usize)
{
	let url = unsafe { CStr::from_ptr(c_url).to_str().unwrap() };
	let arg = unsafe { CStr::from_ptr(c_arg).to_str().unwrap() };

	let ctype = unsafe { CStr::from_ptr(c_ctype).to_str().unwrap() };
	let data = unsafe { std::slice::from_raw_parts_mut(c_data, c_size) };

	ON_POST_HANDLER.with(|handler| handler.borrow()(url, arg, ctype, data));
	default_post_handler(url, arg, ctype, data);
}
pub fn set_backend_post(cb: PostHandler)
{
	ON_POST_HANDLER.with(|handler| *handler.borrow_mut() = cb);
	unsafe {
		asm!("out 0x0, eax",
			in("eax") 0x10000,
			in("rdi") 2,
			in("rsi") sys_on_post
		);
	}
}

#[inline]
#[allow(dead_code)]
pub fn wait_for_requests() -> !
{
	unsafe {
		asm!("out 0x0, eax",
			in("eax") 0x10001,
			options(noreturn)
		);
	}
}

/*
struct backend_request {
	const char *method;
	const char *url;
	const char *arg;
	const char *content_type;
	uint16_t    method_len;
	uint16_t    url_len;
	uint16_t    arg_len;
	uint16_t    content_type_len;
	const uint8_t *content; /* Can be NULL. */
	size_t         content_len;
};
*/
#[allow(dead_code)]
pub struct RequestHeader {
	pub value: *const c_char,
	pub colon_index: u32,
	pub len: u32,
}
#[allow(dead_code)]
pub struct Request {
	pub method: *const c_char,
	pub url: *const c_char,
	pub arg: *const c_char,
	pub content_type: *const c_char,
	pub method_len: u16,
	pub url_len: u16,
	pub arg_len: u16,
	pub content_type_len: u16,
	pub content: *const u8,
	pub content_len: usize,
	pub headers: *const RequestHeader,
	pub num_headers: u16,
	pub flags: u16,
	pub reserved: u32,
	pub reserved2: u64,
	pub reserved3: u64,
}

impl Request {
	#[allow(dead_code)]
	pub fn method(&self) -> &str {
		unsafe { CStr::from_ptr(self.method).to_str().unwrap() }
	}
	#[allow(dead_code)]
	pub fn url(&self) -> &str {
		unsafe { CStr::from_ptr(self.url).to_str().unwrap() }
	}
	#[allow(dead_code)]
	pub fn arg(&self) -> &str {
		unsafe { CStr::from_ptr(self.arg).to_str().unwrap() }
	}
	#[allow(dead_code)]
	pub fn content_type(&self) -> &str {
		unsafe { CStr::from_ptr(self.content_type).to_str().unwrap() }
	}
	#[allow(dead_code)]
	pub fn content(&self) -> &[u8] {
		unsafe { std::slice::from_raw_parts(self.content, self.content_len) }
	}
}

#[inline]
#[allow(dead_code)]
pub fn wait_for_requests_paused() -> Request
{
	unsafe {
		let mut req = MaybeUninit::<Request>::uninit();
		asm!("out 0x0, eax",
			in("eax") 0x10002,
			in("rdi") req.as_mut_ptr(),
		);
		return req.assume_init();
	}
}

/** Debugging and introspection **/

#[inline]
#[allow(dead_code)]
pub fn breakpoint()
{
	unsafe {
		asm!("out 0x0, eax",
			in("eax") 0x7F7F7
		);
	}
}
