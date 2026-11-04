# CI and release notes

The workflow builds from a clean checkout against an explicit SuperCollider
3.14.1 source tree. It covers Linux x86-64/arm64, macOS Intel/arm64, Windows
x86-64, a Linux supernova build, and standalone sanitizer tests.

Each job stages an installable `MIRa/` extension and names artifacts with the
GitHub repository owner, project, version, platform, and architecture. Hosted
jobs are preparation for publication and must pass in the future standalone
repository before a release is tagged; local cross-builds are not substitutes
for hosted platform execution.

