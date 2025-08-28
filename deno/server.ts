type NativePrimitive = Extract<Deno.NativeType, string>;
type NativeToJS<T extends NativePrimitive> = T extends "bool" ? boolean
	: T extends Deno.NativeNumberType ? number
	: bigint;
type Width = 1 | 2 | 4 | 8;

const WIDTH = {
	bool: 1,
	i8: 1,
	u8: 1,
	i16: 2,
	u16: 2,
	i32: 4,
	u32: 4,
	f32: 4,
	i64: 8,
	u64: 8,
	f64: 8,
	isize: 8,
	usize: 8,
	pointer: 8,
	buffer: 8,
	function: 8,
} as const satisfies Record<NativePrimitive, Width>;

const ACCESSOR: {
	[P in NativePrimitive]: (
		this: DataView,
		byteOffset: number,
		littleEndian: boolean,
	) => NativeToJS<P>;
} = {
	bool(this: DataView, byteOffset: number) {
		return this.getUint8(byteOffset) !== 0;
	},
	i8: DataView.prototype.getInt8,
	u8: DataView.prototype.getUint8,
	i16: DataView.prototype.getInt16,
	u16: DataView.prototype.getUint16,
	i32: DataView.prototype.getInt32,
	u32: DataView.prototype.getUint32,
	f32: DataView.prototype.getFloat32,
	i64: DataView.prototype.getBigInt64,
	u64: DataView.prototype.getBigUint64,
	f64: DataView.prototype.getFloat64,
	isize: DataView.prototype.getBigInt64,
	usize: DataView.prototype.getBigUint64,
	pointer: DataView.prototype.getBigUint64,
	buffer: DataView.prototype.getBigUint64,
	function: DataView.prototype.getBigUint64,
};

function structOffsets<
	Spec extends { [name: string]: NativePrimitive },
	Offsets = { [Name in keyof Spec]: number },
>(spec: Spec): { byteLength: number; offsets: Offsets } {
	let byteLength = 0;
	const offsets = Object.fromEntries(
		Object.entries(spec).map(([name, nativeType]) => {
			const width = WIDTH[nativeType];
			const misalignment = byteLength % width;
			if (misalignment !== 0) {
				byteLength += width - misalignment;
			}
			const offset = byteLength;
			byteLength += width;
			return [name, offset];
		}),
	) as Offsets;
	return { byteLength, offsets };
}

type Accessor<T extends NativePrimitive> = (dv: DataView) => NativeToJS<T>;

function struct<
	Spec extends { [name: string]: NativePrimitive },
	Accessors = {
		[Name in keyof Spec & string as `get_raw_${Name}`]: (
			dv: DataView,
		) => NativeToJS<Spec[Name]>;
	},
>(spec: Spec): Accessors & { byteLength: number } {
	const { byteLength, offsets } = structOffsets(spec);
	const accessors = Object.fromEntries(
		Object.entries(spec).map(<T extends NativePrimitive>(
			[name, nativeType]: [string, T],
		) => {
			const offset = offsets[name];
			const accessor = ACCESSOR[nativeType];
			const getterName = `get_raw_${name}`;
			const fn = ({
				[getterName]: (dv: DataView) => accessor.call(dv, offset, true),
			})[getterName];
			return [getterName, fn];
		}),
	) as Accessors;
	return { byteLength, ...accessors };
}

const latin1 = new TextDecoder("latin1");

const bufferAccessor = <
	Name extends string,
	Struct extends
	& { [N in Name as `get_raw_${Name}`]: Accessor<"buffer"> }
	& {
		[N in Name as `get_raw_${Name}_len`]: Accessor<
			Deno.NativeNumberType | Deno.NativeBigIntType
		>;
	},
>(
	name: Name,
	struct: Struct,
) => {
	const accessor = struct[`get_raw_${name}`] as Accessor<"buffer">;
	const accessor_len = struct[`get_raw_${name}_len`] as Accessor<
		Deno.NativeNumberType | Deno.NativeBigIntType
	>;
	return (dv: DataView): Uint8Array | null => {
		const pointer = Deno.UnsafePointer.create(accessor(dv));
		if (pointer === null) {
			return null;
		}
		const byteLength = Number(accessor_len(dv));
		return new Uint8Array(
			Deno.UnsafePointerView.getArrayBuffer(pointer, byteLength),
		);
	};
};

const latin1String =
	// deno-lint-ignore no-explicit-any
	<T extends [...any]>(getBuffer: (...args: T) => Uint8Array | null) =>
		(
			...args: T
		): string => {
			const buffer = getBuffer(...args);
			return buffer === null ? "" : latin1.decode(buffer);
		};

const kvm_request_header = struct(
	{
		field: "pointer", // const char *
		field_colon: "u32", /* Index of the colon in the field. */
		field_len: "u32", /* Length of the entire key: value pair. */
	} as const,
);

const get_field = bufferAccessor("field", kvm_request_header);
const { get_raw_field_colon } = kvm_request_header;

const kvm_request = struct(
	{
		method: "pointer", // const char *
		url: "pointer", // const char *
		arg: "pointer", // const char *
		content_type: "pointer", // const char *
		method_len: "u16",
		url_len: "u16",
		arg_len: "u16",
		content_type_len: "u16",
		content: "pointer", // const uint8_t * /* Can be NULL. */
		content_len: "usize",
		/* HTTP headers */
		headers: "pointer", // struct kvm_request_header *
		num_headers: "u16",
		info_flags: "u16", /* 0x1 = request is a warmup request. */
		reserved0: "u32", /* Reserved for future use. */
		reserved1: "u64", // uint64_t reserved1[2]; /* Reserved for future use. */
		reserved2: "u64",
	} as const,
);

const get_method = latin1String(bufferAccessor("method", kvm_request));
const get_url = latin1String(bufferAccessor("url", kvm_request));
const get_arg = latin1String(bufferAccessor("arg", kvm_request));
const _get_content_type = latin1String(
	bufferAccessor("content_type", kvm_request),
);
const get_content = bufferAccessor("content", kvm_request);
const get_headers = (dv: DataView): Array<[string, string]> => {
	const headers: Array<[string, string]> = [];
	const headerPointer = Deno.UnsafePointer.create(
		kvm_request.get_raw_headers(dv),
	);
	if (headerPointer === null) {
		return headers;
	}
	const num_headers = kvm_request.get_raw_num_headers(dv);
	const headerLength = kvm_request_header.byteLength;
	const headerBuffer = Deno.UnsafePointerView.getArrayBuffer(
		headerPointer,
		num_headers * headerLength,
	);
	for (let i = 0; i < num_headers; i++) {
		const headerView = new DataView(
			headerBuffer,
			i * headerLength,
			headerLength,
		);
		const buffer = get_field(headerView)!;
		const colon = get_raw_field_colon(headerView);
		const name = latin1.decode(buffer.slice(0, colon));
		let start = colon + 1; // space = 32, tab = 9
		while (buffer[start] === 32 || buffer[start] === 9) {
			start += 1;
		}
		const value = latin1.decode(buffer.slice(start));
		headers.push([name, value]);
	}
	return headers;
};
const get_request = (dv: DataView): Request => {
	const url = new URL(get_url(dv), "http://localhost:8080");
	const method = get_method(dv);
	const headers = get_headers(dv);
	const argument = get_arg(dv);
	const body = get_content(dv);
	let req = new Request(url, { method, headers, body });
	req.argument = argument;
	return req;
};

const _ResponseHeader = struct(
	{
		field: "pointer", // const char *
		field_len: "usize",
	} as const,
);

const _BackendResponseExtra = struct(
	{
		headers: "pointer", // const struct ResponseHeader *
		num_headers: "u16",
		cached: "bool",
		ttl: "f32",
		grace: "f32",
		keep: "f32",
		reserved0: "u64", /* Reserved for future use. */
		reserved1: "u64",
		reserved2: "u64",
		reserved3: "u64",
	} as const,
);

// Create /tmp/deno_cache and download libvdeno.so
Deno.mkdirSync("/tmp/deno_cache", { recursive: true });
const req = await fetch("https://filebin.varnish-software.com/4wbvu68xy1epbuzv/libvdeno.so");
const bytes = await req.bytes();
Deno.writeFileSync("/tmp/deno_cache/libvdeno.so", bytes);

const libkvm_api = Deno.dlopen(
	"/tmp/deno_cache/libvdeno.so",
	{
		// extern void __attribute__((used))
		// sys_backend_response(int16_t status, const void *t, size_t, const void *c, size_t,
		//  const struct BackendResponseExtra *extra);
		sys_backend_response: {
			parameters: ["i16", "buffer", "usize", "buffer", "usize", "pointer"],
			result: "void",
		},
		// extern void wait_for_requests_paused(struct kvm_request* req)
		wait_for_requests_paused: { parameters: ["buffer"], result: "void" },
		// extern char *vd_find_header(const char *name)
		vd_find_header: { parameters: ["buffer"], result: "pointer" },
		// extern void vd_set_req_header(const char *full)
		vd_set_req_header: { parameters: ["buffer", "usize"], result: "void" },
		// extern void vd_set_resp_header(const char *full)
		vd_set_resp_header: { parameters: ["buffer", "usize"], result: "void" },
	} as const,
);

export type VarnishAddr = {
	transport: "varnish";
};

export type ServeHandlerInfo<Addr> = {
	remoteAddr: Addr;
	completed: Promise<void>;
};

export type ServeHandler<Addr> = (
	request: Request,
	info: ServeHandlerInfo<Addr>,
) => Response | Promise<Response>;

export const serve = async (handler: ServeHandler<VarnishAddr>) => {
	const remoteAddr: VarnishAddr = { transport: "varnish" };
	while (true) {
		const completed = new Promise<void>(() => { });
		const info = { remoteAddr, completed };
		const reqBuf = new Uint8Array(kvm_request.byteLength);
		const reqView = new DataView(reqBuf.buffer);
		libkvm_api.symbols.wait_for_requests_paused(reqBuf);
		const request = get_request(reqView);
		const response = await handler(request, info);
		const content_type = new TextEncoder().encode(
			response.headers.get("Content-Type") ?? "text/plain",
		);
		const body = await response.bytes();
		libkvm_api.symbols.sys_backend_response(
			response.status,
			content_type,
			BigInt(content_type.byteLength),
			body,
			BigInt(body.byteLength),
			null,
		);
	}
};

export const http_getfield = (name: string): string | null => {
	const nameArray = new TextEncoder().encode(name);
	const headerValue = Deno.UnsafePointerView.getCString(
		libkvm_api.symbols.vd_find_header(nameArray),
	);
	return headerValue;
};

export const http_get = (name: string): [string, string] | null => {
	const headerValue = http_getfield(name);
	if (headerValue === "") {
		return null;
	}
	const [key, value] = headerValue.split(": ");
	return [key, value];
};

export const http_set_req = (name: string, value: string): void => {
	const headerValue = `${name}: ${value}`;
	libkvm_api.symbols.vd_set_req_header(new TextEncoder().encode(headerValue), BigInt(headerValue.length));
};

export const http_set_resp = (name: string, value: string): void => {
	const headerValue = `${name}: ${value}`;
	libkvm_api.symbols.vd_set_resp_header(new TextEncoder().encode(headerValue), BigInt(headerValue.length));
};
