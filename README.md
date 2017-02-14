# L4 HashTable
L4 (Lock-Free on Read) Hashtable is a C++ library that provides a fixed-size hashtable, where keys and values are arbitrary bytes.

L4 HashTable is optimized for lookup operations. It uses [Epoch Queue](https://github.com/Microsoft/L4/wiki/Epoch-Queue) (deterministic garbage collector) to achieve lock-free lookup operations.

L4 HashTable supports caching based on memory size and time. It uses [Clock](https://en.wikipedia.org/wiki/Page_replacement_algorithm#Clock) algorithm for an efficient cache eviction.

L4 HashTable is built with interprocess communication in mind and its shared memory implementation (multiple-readers-single-writer model at process level) is coming soon.

## Installation
This library is developed and maintained with Visual Studio 2015 Update 3. Visual Studio 2015 Community is available for free [here](https://dev.windows.com/downloads).

To get started, open the **L4.sln** file and build the solution. Initially, it will automatically start downloading [boost NuGet package](https://www.nuget.org/packages/boost/) before build starts.

Support for other platforms is under consideration.

## Getting Started
Start with a simple [example](https://github.com/Microsoft/L4/blob/master/Examples/main.cpp).

Check out the [wiki](https://github.com/Microsoft/L4/wiki).

## Contributing

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## License

Code licensed under the [MIT License](https://github.com/Microsoft/L4/blob/master/LICENSE).
