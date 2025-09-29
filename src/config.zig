const std = @import("std");
const clap = @import("clap");

const Error = error {
    invalid_mode,
};

pub const Config = struct {
    alloc: std.mem.Allocator,
    ip: []const u8,
    port: u16,
    topic: []const u8,
    capture_only: bool = false,
    playback_only: bool = false,

    pub fn deinit(self: *Config) void {
        self.alloc.free(self.ip);
        self.alloc.free(self.topic);
    }
};

pub fn config(alloc: std.mem.Allocator) !Config {
    const params = comptime clap.parseParamsComptime(
        \\ -h, --help           Display this help and exit.
        \\ --ip <str>           Connection IP of message bus.
        \\ -p, --port <u16>     Port of the message bus.
        \\ -t, --topic <str>    Topic to connect to.
        \\ --capture_only       Start the application as capture only.
        \\ --playback_only      Start the application as playback only.
    );
    var diag = clap.Diagnostic{};
    var res = clap.parse(clap.Help, &params, clap.parsers.default, .{
        .allocator = alloc,
        .diagnostic = &diag,
    }) catch |err| {
        try diag.reportToFile(.stderr(), err);
        return err;
    };
    defer res.deinit();

    var conf: Config = .{
        .alloc = alloc,
        .ip = try alloc.dupe(u8, "127.0.0.1"),
        .port = 3000,
        .topic = try alloc.dupe(u8, "test"),
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
    if (res.args.capture_only != 0) {
        conf.capture_only = true;
    }
    if (res.args.playback_only != 0) {
        conf.playback_only = true;
    }
    if (conf.capture_only and conf.playback_only) {
        std.log.info("capture_only and playback_only flags cannot be both set at the same time.\n", .{});
        return Error.invalid_mode;
    }
    std.log.info("configuration loaded: ip = {s}, port = {}, topic = {s}, capture_only = {}, playback_only = {}\n", .{conf.ip, conf.port, conf.topic, conf.capture_only, conf.playback_only});
    return conf;
}
