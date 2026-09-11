# Changelog

All notable changes to this project will be documented in this file.
The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.0.0] - 2026-05-01
### Added
- Comprehensive Doxygen comments and inline documentation across the codebase.
- Finalized `README.md` with complete protocol documentation, architecture details, and FSM diagrams.
- Created unit tests and integration tests.
### Changed
- Refactored error handling: replaced "magic number" return values with standard `<sysexits.h>` macros for robust and professional exit codes.
### Fixed
- Resolved duplicated timeout bugs during packet wait cycles.
- Fixed minor issues in networking tests.

## [0.10.0] - 2026-04-28
### Added
- Fully integrated and functional Go-Back-N (GBN) sliding window protocol implementation.
- Basic terminal testing functionality to verify reliable data transfer.
### Changed
- Prepared codebase structure for automated testing and documentation phases.

## [0.9.0] - 2026-04-20
### Added
- Created skeleton for the Go-Back-N (sliding window) protocol (`window.h` and `window.c`).

## [0.8.0] - 2026-04-19
### Added
- Finished Client and Server FSMs.
- Implemented basic Retransmission mechanism (currently built on a Stop-and-Wait system) in preparation for Go-Back-N.
### Changed
- Refactored client and server code, modularizing re-usable code into separate functions.
- Separated client FSM logic into isolated static functions for each state.
- Refactored server logic for better readability and maintainability.
### Fixed
- Resolved pipelining bugs on the server side.

## [0.7.0] - 2026-04-18
### Changed
- Massive debugging session and testing of the first pipelining and Stop-and-Wait implementation.
- Prepared codebase architecture for the upcoming Go-Back-N refactor.

## [0.6.0] - 2026-04-17
### Added
- Hostname resolution progress integration in the client.
- Implemented the first two states of the client FSM.
- Added a non-blocking packet waiting function using `select()` with a timeout to support retransmissions (preventing infinite waiting).
- Created server socket initialization functions.
- Added connection memory structures.
- Implemented a prototype of the 3-way handshake.
### Fixed
- Fixed early development bugs in the client logic.

## [0.5.0] - 2026-04-15
### Added
- Finished the packet parsing function.
- Designed Finite State Machine (FSM) states for both the client and the server.
- Re-used the checksum calculation function from the first project.
- Started implementation of core client functions.

## [0.4.0] - 2026-04-14
### Added
- Packet builder logic.
- Implemented the 15-byte custom RDT header.
- Started working on the packet parsing function.

## [0.3.0] - 2026-04-13
### Added
- Setup error messaging system.
- Designed and initialized the configuration structure (`AppConfig`) for future modular approach.

## [0.2.0] - 2026-04-12
### Added
- Skeleton for the CLI argument parser.
- Added comprehensive `HELP` message output.

## [0.1.0] - 2026-04-11
### Added
- Initial commit: project structure setup.
- Basic `Makefile` implementation.
- MIT License.