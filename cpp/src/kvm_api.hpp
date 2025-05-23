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
#include "kvm_api.h"
#include <array>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

struct Backend
{
    static void response(uint16_t status, std::string_view ct, const void *cont, size_t cont_len)
    {
        backend_response(status, ct.begin(), ct.size(), cont, cont_len);
    }
    static void response(uint16_t status, std::string_view ct, const void *cont, size_t cont_len,
        const std::vector<std::string> &headers, float ttl = 0.0f, float grace = 0.0f, float keep = 0.0f)
    {
        std::array<ResponseHeader, 64> header_array;
        size_t i = 0;
        for (const auto& header : headers) {
            header_array.at(i).field = header.data();
            header_array.at(i).field_len = header.size();
            i++;
        }
        BackendResponseExtra extra = {};
        extra.headers = header_array.data();
        extra.num_headers = i;
        extra.cached = ttl > 0.0f;
        extra.ttl = ttl;
        extra.grace = grace;
        extra.keep = keep;
        backend_response_extra(status, ct.begin(), ct.size(), cont, cont_len, &extra);
    }

    static void response(uint16_t status, std::string_view ct, std::string_view cont)
    {
        backend_response(status, ct.begin(), ct.size(), cont.begin(), cont.size());
    }
    static void response(uint16_t status, std::string_view ct, std::string_view cont,
        const std::vector<std::string> &headers, float ttl = 0.0f, float grace = 0.0f, float keep = 0.0f)
    {
        response(status, ct, cont.begin(), cont.size(), headers, ttl, grace, keep);
    }
};

struct Storage
{
    template <size_t Size = 16UL << 20> /* 16MB buffer */
static std::string get(std::string_view arg, storage_func func)
{
    std::string buffer;
    buffer.resize(Size);
    const long len = storage_call(func, arg.begin(), arg.size(), buffer.data(), buffer.size());
    if (len >= 0)
    {
        buffer.resize(len);
        return buffer;
    }
    throw std::runtime_error("Storage access failed");
}

    template <size_t Size = 16UL << 20> /* 16MB buffer */
    static std::string get(const std::vector<std::string>& arg, storage_func func)
    {
		std::vector<virtbuffer> vbufs(arg.size());
		for (size_t i = 0; i < arg.size(); i++)
		{
			vbufs[i].data = arg[i].data();
			vbufs[i].len  = arg[i].size();
		}

        std::string buffer;
		buffer.resize(Size);
        const long len = storage_callv(func, vbufs.size(), vbufs.data(), buffer.data(), buffer.size());
        if (len >= 0) {
			buffer.resize(len);
			return buffer;
		}
        throw std::runtime_error("Storage access failed");
    }

    template <size_t Size, typename... Args>
    static std::string_view get(storage_func func, char output[Size], Args&&... args)
    {
        virtbuffer vbufs[sizeof...(args)];
        std::size_t i = 0;
        (void(vbufs[i++] = virtbuffer{ .data = args, .size = strlen(args) }) , ...);

        const long len = storage_callv(func, sizeof...(args), vbufs, output, Size);
        if (len >= 0) {
            return std::string_view(output, len);
		}
        throw std::runtime_error("Storage access failed");
    }

	static void task(storage_task_func func, std::string_view buffer)
	{
		storage_task(func, buffer.begin(), buffer.size());
	}
	static void task(storage_task_func func, const void* arg = nullptr, size_t len = 0)
	{
		storage_task(func, arg, len);
	}
};

struct Http
{
    static void append(int hp, std::string_view text)
    {
        sys_http_append(hp, text.begin(), text.size());
    }
    static void set(int hp, std::string_view text)
    {
        sys_http_set(hp, text.begin(), text.size());
    }
    static std::string get(int hp, const std::string &name)
    {
        std::string result(1024, 'x');
        const unsigned len =
            sys_http_find(hp, name.c_str(), name.size(), result.data(), result.size());
        result.resize(len);
        return result;
    }
};

struct Curl
{
    struct Result
    {
        uint16_t status;
        std::string_view content_type;
        std::string_view content;

        bool failed() const noexcept { return status == 0; }
    };
    using FieldList = std::vector<std::string>;

    static Result fetch(std::string_view url, const FieldList& strvec = {}, curl_options* options = nullptr)
    {
        /* Static in order to return a string_view of the content-type. */
        static struct curl_op op;
        op = {};
        struct curl_fields fields = {};
        if (!strvec.empty()) {
            size_t i = 0;
            for (const auto& str : strvec) {
                fields.ptr[i] = str.c_str();
                fields.len[i] = str.size();
                i++;
            }
        }
        if (sys_fetch(url.begin(), url.size(), &op, &fields, options) == 0)
        {
            return Result{
                .status = (uint16_t)op.status,
                .content_type = {op.ctype, op.ctlen},
                .content = {(const char *)op.content, op.content_length}
            };
        }
        return Result{
            .status = 0,
            .content_type = "",
            .content = ""
        };
    }
    static Result head(std::string_view url, const FieldList& strvec = {})
    {
        struct curl_options options = {};
        options.dummy_fetch = true;
        return fetch(url, strvec, &options);
    }
    static Result post(const std::string& url,
        const std::string& ctype, const std::string_view data,
        const FieldList& strvec = {})
    {
        struct curl_op op = {};
        struct curl_fields fields = {};
        if (!strvec.empty()) {
            size_t i = 0;
            for (const auto& str : strvec) {
                fields.ptr[i] = str.c_str();
                fields.len[i] = str.size();
                i++;
            }
        }
        op.post_buffer = data.begin();
        op.post_buflen = data.size();
        std::memcpy(op.ctype, ctype.c_str(), ctype.size());
        op.ctlen = ctype.size();
        if (sys_fetch(url.c_str(), url.size(), &op, &fields, NULL) == 0)
        {
            return Result{
                .status = (uint16_t)op.status,
                .content_type = {op.ctype, op.ctlen},
                .content = {(const char *)op.content, op.content_length}
            };
        }
        return Result{
            .status = 0,
            .content_type = "",
            .content = ""
        };
    }

    static Result self(std::string_view url, const FieldList& strvec = {}, curl_options* options = nullptr)
    {
        /* Static in order to return a string_view of the content-type. */
        static struct curl_op op;
        op = {};
        struct curl_fields fields = {};
        if (!strvec.empty()) {
            size_t i = 0;
            for (const auto& str : strvec) {
                fields.ptr[i] = str.c_str();
                fields.len[i] = str.size();
                i++;
            }
        }
        if (sys_request(url.begin(), url.size(), &op, &fields, options) == 0)
        {
            return Result{
                .status = (uint16_t)op.status,
                .content_type = {op.ctype, op.ctlen},
                .content = {(const char *)op.content, op.content_length}
            };
        }
        return Result{
            .status = 0,
            .content_type = "",
            .content = ""
        };
    }
};
