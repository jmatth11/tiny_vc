const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const mod = b.addModule("tiny_vc", .{
        .root_source_file = b.path("src/root.zig"),
        .target = target,
    });

    const dep_opts = .{.target = target, .optimize = optimize,};
    const chebi = b.dependency("chebi", dep_opts).module("chebi");
    const clap = b.dependency("clap", dep_opts).module("clap");

    const exe = b.addExecutable(.{
        .name = "tiny_vc",
        .root_module = b.createModule(.{
            .root_source_file = b.path("src/main.zig"),
            .target = target,
            .optimize = optimize,
            .imports = &.{
                // Here "tiny_vc" is the name you will use in your source code to
                // import this module (e.g. `@import("tiny_vc")`). The name is
                // repeated because you are allowed to rename your imports, which
                // can be extremely useful in case of collisions (which can happen
                // importing modules from different packages).
                .{ .name = "tiny_vc", .module = mod },
                .{ .name = "chebi", .module = chebi },
                .{ .name = "clap", .module = clap },
            },
        }),
    });

    b.installArtifact(exe);

    const run_step = b.step("run", "Run the app");

    const run_cmd = b.addRunArtifact(exe);
    run_step.dependOn(&run_cmd.step);
    run_cmd.step.dependOn(b.getInstallStep());

    if (b.args) |args| {
        run_cmd.addArgs(args);
    }

    const mod_tests = b.addTest(.{
        .root_module = mod,
    });

    const run_mod_tests = b.addRunArtifact(mod_tests);

    const exe_tests = b.addTest(.{
        .root_module = exe.root_module,
    });

    const run_exe_tests = b.addRunArtifact(exe_tests);

    const test_step = b.step("test", "Run tests");
    test_step.dependOn(&run_mod_tests.step);
    test_step.dependOn(&run_exe_tests.step);
}
