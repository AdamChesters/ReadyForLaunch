# Testing and limitations

Build the Windows Release configuration and run `ctest -C Release --output-on-failure`
from the build directory. The repository's tests cover scheduling and profiles,
Windows process lifecycle, update verification and UI interactions where provided
by that revision. Use isolated fixtures rather than a user's running applications.

Automated tests do not certify every application launch, graceful shutdown,
interactive installer update, mixed-DPI display or flight simulator/headset sequence.
Validate the actual app stack and preserve profile backups before relying on it.
Process discovery alone does not establish readiness or successful launch.

See [the user guide](USER-GUIDE.md) for supported behavior and [build instructions](../README.md#build).
