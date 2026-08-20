(
var projectRoot = thisProcess.nowExecutingPath.dirname.dirname.dirname;
var paths = [
    projectRoot +/+ "tests/sc/compile_check.scd",
    projectRoot +/+ "tests/sc/collision_check.scd",
    projectRoot +/+ "tests/sc/compile_help_examples.scd",
    projectRoot +/+ "tests/sc/verify_scdoc.scd",
    projectRoot +/+ "tests/sc/live_smoke.scd",
    projectRoot +/+ "tests/sc/randomized_safety.scd",
    projectRoot +/+ "tests/sc/supernova_smoke.scd",
    projectRoot +/+ "tests/sc/nrt_matrix.scd",
    projectRoot +/+ "benchmarks/realtime_cpu.scd",
    projectRoot +/+ "scripts/pitch_compare.scd"
];
var ok = true;

paths.do { |path|
    if(File.readAllString(path).compile.isNil) {
        ("TEST_SCRIPT_COMPILE_FAILED " ++ path).postln;
        ok = false
    }
};

if(ok) {
    "MIR2_TEST_SCRIPTS_COMPILE_OK".postln;
    0.exit
} {
    1.exit
};
)

