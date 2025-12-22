# Vigine (Virtual engine)

Vigine is a virtual engine designed for rapid development of C++ programs. It provides a modular architecture to streamline the creation of applications by managing states, tasks, services, and entities.

## Architecture

Vigine follows a component-based architecture that separates concerns and promotes reusability. The key components are:

- **Vigine**: The main engine class that orchestrates the entire application lifecycle.
- **State**: Represents different states of the program, allowing for state-based logic and transitions.
- **TaskFlow**: Manages the flow of task execution, enabling sequential or conditional task processing.
- **Task**: Individual units of work executed by the engine, encapsulating specific operations.
- **Service**: High-level APIs that utilize underlying systems to provide functionality for specific domains (e.g., graphics, database).
- **System**: Core components within services that contain the business logic and handle low-level operations.
- **Entity**: Objects that systems interact with, serving as containers for components.
- **Component**: Data structures attached to entities, holding data with minimal logic for data management and access.

This architecture allows for flexible and scalable application development, making it easier to build complex C++ programs efficiently.

## Building

Vigine uses CMake for build configuration. To build the project:

1. Ensure you have CMake installed.
2. Clone the repository and navigate to the project directory.
3. Run the following commands:

```bash
mkdir build
cd build
cmake ..
make
```

## Usage

Include Vigine in your C++ project and use the provided classes to structure your application. Refer to the `example/` directory for sample usage.

## Contributing

Contributions are welcome! Please submit issues and pull requests to the [GitHub repository](https://github.com/vigine-dev/Vigine).

## License

This project is licensed under the Apache License 2.0. See the LICENSE file for details.