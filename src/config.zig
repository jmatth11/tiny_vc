const std = @import("std");
const clap = @import("clap");

pub const Config = struct {
    ip: []const u8,
    port: u16,
    topic: []const u8,
};

pub fn config(alloc: std.mem.Allocator) !Config {
    const params = comptime clap.parseParamsComptime(
        \\ -h, --help      Display this help and exit.
        \\ -ip             Connection IP of message bus.
        \\ -p, -port           Port of the message bus.
        \\ -t, --topic     Topic to connect to.
    );
    var diag = clap.Diagnostic{};
    var res = clap.parse(clap.Help, &params, clap.parsers.default, .{
        .allocator = alloc,
        .diagnostic = diag,
    }) catch |err| {
        try diag.reportToFile(.stderr(), err);
        return err;
    };
    defer res.deinit();

    var conf: Config = .{
        .ip = "127.0.0.1",
        .port = 3000,
        .topic = "test",
    };

    if (res.args.help != 0) {
        std.debug.print("--help\n", .{});
    }
    if (res.args.ip) |ip| {
        conf.ip = try alloc.dupe(u8, ip);
    }
    if (res.args.port) |port| {
        conf.port = port;
    }
    if (res.args.topic) |topic| {
        conf.topic = try alloc.dupe(u8, topic);
    }

    return conf;
}
