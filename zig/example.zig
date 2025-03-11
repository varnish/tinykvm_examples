const std = @import("std");
const bufPrint = std.fmt.bufPrint;
const varnish = @cImport({
    @cInclude("kvm_api.h");
});

fn my_storage(_: usize, _: [*c]varnish.struct_virtbuffer, _: usize) callconv(.C) void {
    varnish.storage_return("Hello Storage", 13);
}

fn on_get(c_url: [*c]const u8, _: [*c]const u8) callconv(.C) void {
    const url = std.mem.sliceTo(c_url, 0);
    if (std.mem.eql(u8, url, "/storage")) {
        var buffer: [4096]u8 = undefined;
        const len: isize = varnish.storage_call(my_storage, null, 0, &buffer, buffer.len);

        _ = varnish.set_cacheable(false, 0.0, 0.0, 0.0);
        varnish.backend_response(200, "text/plain", 10, &buffer, @intCast(len));
    }

    var minfo: varnish.meminfo = undefined;
    varnish.get_meminfo(&minfo);

    var buf: [64]u8 = undefined;
    const xmu = bufPrint(&buf, "X-Memory-Usage: {}", .{std.fmt.fmtIntSizeBin(minfo.reqmem_current)}) catch {
        return;
    };

    _ = varnish.sys_http_append(varnish.RESP, xmu.ptr, xmu.len);

    _ = varnish.set_cacheable(false, 0.0, 0.0, 0.0);
    varnish.backend_response_str(200, "text/plain", "Hello World!");
}

pub fn main() !void {
    const stdout = std.io.getStdOut().writer();
    try stdout.print("Hello, {s}!\n", .{"world"});

    varnish.set_backend_get(on_get);
    varnish.wait_for_requests();
}
