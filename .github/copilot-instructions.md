
## Project Overview
- Consists mainly of shell scripts and C code that will run during initialization or house keeping of embedded systems.
- The software will be deployed on a variety of embedded systems.
- These systems have limited memory (ranging from a few KBs to a few MBs).
- CPU resources are constrained, often with low clock speeds.
- Real-time performance may be required in some cases.
- The environment may lack standard OS features like file systems or dynamic memory allocation.
- Cross-compilation will be used for building the software.
- Multiple architectures and compiler toolchains must be supported.
- Software must be platform-neutral and portable.
- Software should be easy to maintain, extendable, and should follow modular design principles.
- Use fixed-point arithmetic where possible instead of floating-point.
- Ensure thread safety, thread pooling if applicable.
- Avoid using dynamic memory allocation to best extent possible. Use memory pools if applicable.
- Provide clear error handling and reporting mechanisms.

## Folder Structure
- All source files will be placed in the `src/` directory.
- All header files will be placed in the `include/` directory.
- All unit tests will be placed in the `src/test/` directory.
- All documentation will be placed in the `docs/` directory.

## Available opensource components and libraries that can be used 
- List of opensource libraries that could used is available in - https://github.com/rdkcentral/meta-oss-reference-release/tree/main#components-details-in-packagegroup-oss-layer

## Security Considerations
- Follow secure coding practices to prevent common vulnerabilities.
- Validate all inputs rigorously.
- Manage memory safely to avoid leaks and overflows.
- Implement authentication and authorization where applicable.
- Encrypt sensitive data in transit and at rest.
- Keep third-party dependencies up to date and minimal.
- Conduct regular security reviews and testing.

## Documentation Guidelines
- Use Markdown format for easy readability.
- Dependencies and versioning should be clearly documented in `docs/DEPENDENCIES.md`.

## Unit Testing
- Unit tests will be placed in the src/test directory.
- Use Google Test and Google Mock frameworks.
- Aim for test coverage above 80%.
- Include tests for edge cases and error conditions.
- Automate tests using a CI/CD pipeline and github workflows.
- Unit tests should be performed on containerized environment using docker image - https://github.com/rdkcentral/docker-rdk-ci/pkgs/container/docker-rdk-ci

## Task Specific Instructions/Prompts
- Task specific instructions could be added in `.github/instructions/*task-features*.md` file.


## Folder Structure

- All source files will be linted using astyle with the configuration file located at `.astyle.rc`.
- Use autotools for build configuration and Makefiles for compilation.
- Use `gcc` as the primary compiler, ensuring compatibility with `clang` where possible.

