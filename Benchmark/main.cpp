#include "L4/LocalMemory/HashTableService.h"
#include "L4/Log/PerfCounter.h"

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <string>
#include <thread>
#include <boost/any.hpp>
#include <boost/program_options.hpp>

class Timer
{
public:
    Timer()
        : m_start{ std::chrono::high_resolution_clock::now() }
    {}

    void Reset()
    {
        m_start = std::chrono::high_resolution_clock::now();
    }

    std::chrono::microseconds GetElapsedTime()
    {
        return std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::high_resolution_clock::now() - m_start);
    }

private:
    std::chrono::time_point<std::chrono::steady_clock> m_start;
};


class SynchronizedTimer
{
public:
    SynchronizedTimer() = default;

    void Start()
    {
        if (m_isStarted)
        {
            return;
        }
        m_isStarted = true;
        m_startCount = std::chrono::high_resolution_clock::now().time_since_epoch().count();
        
    }

    void End()
    {
        m_endCount = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    }

    std::chrono::microseconds GetElapsedTime()
    {
        std::chrono::nanoseconds start{ m_startCount };
        std::chrono::nanoseconds end{ m_endCount };
        
        return std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    }

private:
    std::atomic_bool m_isStarted = false;
    std::atomic_uint64_t m_startCount;
    std::atomic_uint64_t m_endCount;
};


struct PerThreadInfoForWriteTest
{
    std::thread m_thread;
    std::size_t m_dataSetSize = 0;
    std::chrono::microseconds m_totalTime;
};


struct PerThreadInfoForReadTest
{
    std::thread m_thread;
    std::size_t m_dataSetSize = 0;
    std::chrono::microseconds m_totalTime;
};


struct CommandLineOptions
{
    static constexpr std::size_t c_defaultDataSetSize = 1000000;
    static constexpr std::uint32_t c_defaultNumBuckets = 1000000;
    static constexpr std::uint16_t c_defaultKeySize = 16;
    static constexpr std::uint32_t c_defaultValueSize = 100;
    static constexpr bool c_defaultRandomizeValueSize = false;
    static constexpr std::uint32_t c_defaultNumIterationsPerGetContext = 1;
    static constexpr std::uint16_t c_defaultNumThreads = 1;
    static constexpr std::uint32_t c_defaultEpochProcessingIntervalInMilli = 10;
    static constexpr std::uint16_t c_defaultNumActionsQueue = 1;
    static constexpr std::uint32_t c_defaultRecordTimeToLiveInSeconds = 300;
    static constexpr std::uint64_t c_defaultCacheSizeInBytes = 1024 * 1024 * 1024;
    static constexpr bool c_defaultForceTimeBasedEviction = false;
    
    std::string m_module;
    std::size_t m_dataSetSize = 0;
    std::uint32_t m_numBuckets = 0;
    std::uint16_t m_keySize = 0;
    std::uint32_t m_valueSize = 0;
    bool m_randomizeValueSize = false;
    std::uint32_t m_numIterationsPerGetContext = 0;
    std::uint16_t m_numThreads = 0;
    std::uint32_t m_epochProcessingIntervalInMilli;
    std::uint8_t m_numActionsQueue = 0;

    // The followings are specific for cache hash tables.
    std::uint32_t m_recordTimeToLiveInSeconds = 0U;
    std::uint64_t m_cacheSizeInBytes = 0U;
    bool m_forceTimeBasedEviction = false;

    bool IsCachingModule() const
    {
        static const std::string c_cachingModulePrefix{ "cache" };
        return m_module.substr(0, c_cachingModulePrefix.size()) == c_cachingModulePrefix;
    }
};


class DataGenerator
{
public:
    DataGenerator(
        std::size_t dataSetSize,
        std::uint16_t keySize,
        std::uint32_t valueSize,
        bool randomizeValueSize,
        bool isDebugMode = false)
        : m_dataSetSize{ dataSetSize }
        , m_keySize{ keySize }
    {
        if (isDebugMode)
        {
            std::cout << "Generating data set with size = " << dataSetSize << std::endl;
        }

        Timer timer;
        
        // Populate keys.
        m_keys.resize(m_dataSetSize);
        m_keysBuffer.resize(m_dataSetSize);
        for (std::size_t i = 0; i < m_dataSetSize; ++i)
        {
            m_keysBuffer[i].resize(keySize);
            std::generate(m_keysBuffer[i].begin(), m_keysBuffer[i].end(), std::rand);
            std::snprintf(reinterpret_cast<char*>(m_keysBuffer[i].data()), keySize, "%llu", i);
            m_keys[i].m_data = m_keysBuffer[i].data();
            m_keys[i].m_size = m_keySize;
        }

        // Populate values buffer. Assumes srand() is already called.
        std::generate(m_valuesBuffer.begin(), m_valuesBuffer.end(), std::rand);

        // Populate values.
        m_values.resize(m_dataSetSize);
        std::size_t currentIndex = 0;
        for (std::size_t i = 0; i < m_dataSetSize; ++i)
        {
            m_values[i].m_data = &m_valuesBuffer[currentIndex % c_valuesBufferSize];
            m_values[i].m_size = randomizeValueSize ? rand() % valueSize : valueSize;
            currentIndex += valueSize;
        }

        if (isDebugMode)
        {
            std::cout << "Finished generating data in "
                << timer.GetElapsedTime().count() << " microseconds" << std::endl;
        }
    }

    L4::IReadOnlyHashTable::Key GetKey(std::size_t index) const
    {
        return m_keys[index % m_dataSetSize];
    }

    L4::IReadOnlyHashTable::Value GetValue(std::size_t index) const
    {
        return m_values[index % m_dataSetSize];
    }

private:
    std::size_t m_dataSetSize;
    std::uint16_t m_keySize;

    std::vector<std::vector<std::uint8_t>> m_keysBuffer;
    std::vector<L4::IReadOnlyHashTable::Key> m_keys;
    std::vector<L4::IReadOnlyHashTable::Value> m_values;

    static const std::size_t c_valuesBufferSize = 64 * 1024;
    std::array<std::uint8_t, c_valuesBufferSize> m_valuesBuffer;
};


void PrintHardwareInfo()
{
    SYSTEM_INFO sysInfo;
    GetSystemInfo(&sysInfo);

    printf("\n");
    printf("Hardware information: \n");
    printf("-------------------------------------\n");
    printf("%22s | %10u |\n", "OEM ID", sysInfo.dwOemId);
    printf("%22s | %10u |\n", "Number of processors", sysInfo.dwNumberOfProcessors);
    printf("%22s | %10u |\n", "Page size", sysInfo.dwPageSize);
    printf("%22s | %10u |\n", "Processor type", sysInfo.dwProcessorType);
    printf("-------------------------------------\n");
    printf("\n");
}


void PrintOptions(const CommandLineOptions& options)
{
    printf("------------------------------------------------------\n");

    printf("%39s | %10llu |\n", "Data set size", options.m_dataSetSize);
    printf("%39s | %10lu |\n", "Number of hash table buckets", options.m_numBuckets);
    printf("%39s | %10lu |\n", "Key size", options.m_keySize);
    printf("%39s | %10lu |\n", "Value type", options.m_valueSize);
    printf("%39s | %10lu |\n", "Number of iterations per GetContext()", options.m_numIterationsPerGetContext);
    printf("%39s | %10lu |\n", "Epoch processing interval (ms)", options.m_epochProcessingIntervalInMilli);
    printf("%39s | %10lu |\n", "Number of actions queue", options.m_numActionsQueue);

    if (options.IsCachingModule())
    {
        printf("%39s | %10lu |\n", "Record time to live (s)", options.m_recordTimeToLiveInSeconds);
        printf("%39s | %10llu |\n", "Cache size in bytes", options.m_cacheSizeInBytes);
        printf("%39s | %10lu |\n", "Force time-based eviction", options.m_forceTimeBasedEviction);
    }

    printf("------------------------------------------------------\n\n");
}


void PrintHashTableCounters(const L4::HashTablePerfData& perfData)
{
    printf("HashTableCounter:\n");
    printf("----------------------------------------------------\n");
    for (auto i = 0; i < static_cast<std::uint16_t>(L4::HashTablePerfCounter::Count); ++i)
    {
        printf("%35s | %12llu |\n",
            L4::c_hashTablePerfCounterNames[i],
            perfData.Get(static_cast<L4::HashTablePerfCounter>(i)));
    }
    printf("----------------------------------------------------\n\n");
}


L4::HashTableConfig CreateHashTableConfig(const CommandLineOptions& options)
{
    return L4::HashTableConfig(
        "Table1",
        L4::HashTableConfig::Setting{ options.m_numBuckets },
        options.IsCachingModule()
        ? boost::optional<L4::HashTableConfig::Cache>{
        L4::HashTableConfig::Cache{
            options.m_cacheSizeInBytes,
            std::chrono::seconds{ options.m_recordTimeToLiveInSeconds },
            options.m_forceTimeBasedEviction }}
    : boost::none);
}


L4::EpochManagerConfig CreateEpochManagerConfig(const CommandLineOptions& options)
{
    return L4::EpochManagerConfig(
        10000U,
        std::chrono::milliseconds(options.m_epochProcessingIntervalInMilli),
        options.m_numActionsQueue);
}


void ReadPerfTest(const CommandLineOptions& options)
{
    printf("Performing read-perf which reads all the records inserted:\n");

    PrintOptions(options);

    auto dataGenerator = std::make_unique<DataGenerator>(
        options.m_dataSetSize,
        options.m_keySize,
        options.m_valueSize,
        options.m_randomizeValueSize);

    L4::LocalMemory::HashTableService service(CreateEpochManagerConfig(options));
    const auto hashTableIndex = service.AddHashTable(CreateHashTableConfig(options));

    // Insert data set.
    auto context = service.GetContext();
    auto& hashTable = context[hashTableIndex];

    std::vector<std::uint32_t> randomIndices(options.m_dataSetSize);
    for (std::uint32_t i = 0U; i < options.m_dataSetSize; ++i)
    {
        randomIndices[i] = i;
    }
    if (options.m_numThreads > 0)
    {
        // Randomize index only if multiple threads are running
        // not to skew the results.
        std::random_shuffle(randomIndices.begin(), randomIndices.end());
    }

    for (int i = 0; i < options.m_dataSetSize; ++i)
    {
        auto key = dataGenerator->GetKey(randomIndices[i]);
        auto val = dataGenerator->GetValue(randomIndices[i]);

        hashTable.Add(key, val);
    }

    std::vector<PerThreadInfoForReadTest> allInfo;
    allInfo.resize(options.m_numThreads);

    SynchronizedTimer overallTimer;
    std::mutex mutex;
    std::condition_variable cv;
    const auto isCachingModule = options.IsCachingModule();
    bool isReady = false;

    const std::size_t dataSetSizePerThread = options.m_dataSetSize / options.m_numThreads;
    for (std::uint16_t i = 0; i < options.m_numThreads; ++i)
    {
        auto& info = allInfo[i];

        std::size_t startIndex = i * dataSetSizePerThread;
        info.m_dataSetSize = (i + 1 == options.m_numThreads)
            ? options.m_dataSetSize - startIndex
            : dataSetSizePerThread;

        info.m_thread = std::thread([=, &service, &dataGenerator, &info, &mutex, &cv, &isReady, &overallTimer]
        {
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [&] { return isReady == true; });
            }

            overallTimer.Start();

            Timer totalTimer;
            Timer getTimer;

            std::size_t iteration = 0;
            bool isDone = false;

            while (!isDone)
            {
                auto context = service.GetContext();
                auto& hashTable = context[hashTableIndex];

                for (std::uint32_t j = 0; !isDone && j < options.m_numIterationsPerGetContext; ++j)
                {
                    auto key = dataGenerator->GetKey(startIndex + iteration);
                    L4::IReadOnlyHashTable::Value val;

                    if (!hashTable.Get(key, val) && !isCachingModule)
                    {
                        throw std::runtime_error("Look up failure is not allowed in this test.");
                    }

                    isDone = (++iteration == info.m_dataSetSize);
                }
            }

            overallTimer.End();

            info.m_totalTime = totalTimer.GetElapsedTime();
        });
    }

    {
        std::unique_lock<std::mutex> lock(mutex);
        isReady = true;
    }

    // Now, start the benchmarking for all threads.
    cv.notify_all();

    for (auto& info : allInfo)
    {
        info.m_thread.join();
    }

    PrintHashTableCounters(service.GetContext()[hashTableIndex].GetPerfData());

    printf("Result:\n");
    printf("            |            Total             |               |\n");
    printf("            |  micros/op  |  microseconds  |  DataSetSize  |\n");
    printf(" -----------------------------------------------------------\n");

    for (std::size_t i = 0; i < allInfo.size(); ++i)
    {
        const auto& info = allInfo[i];

        printf(" Thread #%llu  | %11.3f | %14llu | %13llu |\n",
            (i + 1),
            static_cast<double>(info.m_totalTime.count()) / info.m_dataSetSize,
            info.m_totalTime.count(),
            info.m_dataSetSize);
    }
    printf(" -----------------------------------------------------------\n");

    printf(" Overall    | %11.3f | %14llu | %13llu |\n",
        static_cast<double>(overallTimer.GetElapsedTime().count()) / options.m_dataSetSize,
        overallTimer.GetElapsedTime().count(),
        options.m_dataSetSize);
}


void WritePerfTest(const CommandLineOptions& options)
{
    if (options.m_module == "overwrite-perf")
    {
        printf("Performing overwrite-perf (writing data with unique keys, then overwrite data with same keys):\n");
    }
    else
    {
        printf("Performing write-perf (writing data with unique keys):\n");
    }

    PrintOptions(options);

    auto dataGenerator = std::make_unique<DataGenerator>(
        options.m_dataSetSize,
        options.m_keySize,
        options.m_valueSize,
        options.m_randomizeValueSize);

    L4::LocalMemory::HashTableService service(CreateEpochManagerConfig(options));
    const auto hashTableIndex = service.AddHashTable(CreateHashTableConfig(options));

    if (options.m_module == "overwrite-perf")
    {
        std::vector<std::uint32_t> randomIndices(options.m_dataSetSize);
        for (std::uint32_t i = 0U; i < options.m_dataSetSize; ++i)
        {
            randomIndices[i] = i;
        }
        if (options.m_numThreads > 0)
        {
            // Randomize index only if multiple threads are running
            // not to skew the results.
            std::random_shuffle(randomIndices.begin(), randomIndices.end());
        }

        auto context = service.GetContext();
        auto& hashTable = context[hashTableIndex];

        for (int i = 0; i < options.m_dataSetSize; ++i)
        {
            const auto index = randomIndices[i];
            auto key = dataGenerator->GetKey(index);
            auto val = dataGenerator->GetValue(index);

            hashTable.Add(key, val);
        }
    }

    std::vector<PerThreadInfoForWriteTest> allInfo;
    allInfo.resize(options.m_numThreads);

    SynchronizedTimer overallTimer;
    std::mutex mutex;
    std::condition_variable cv;
    bool isReady = false;

    const std::size_t dataSetSizePerThread = options.m_dataSetSize / options.m_numThreads;
    for (std::uint16_t i = 0; i < options.m_numThreads; ++i)
    {
        auto& info = allInfo[i];

        std::size_t startIndex = i * dataSetSizePerThread;
        info.m_dataSetSize = (i + 1 == options.m_numThreads)
            ? options.m_dataSetSize - startIndex
            : dataSetSizePerThread;
        
        info.m_thread = std::thread([=, &service, &dataGenerator, &info, &mutex, &cv, &isReady, &overallTimer]
        {
            {
                std::unique_lock<std::mutex> lock(mutex);
                cv.wait(lock, [&] { return isReady == true; });
            }

            overallTimer.Start();

            Timer totalTimer;
            Timer addTimer;

            std::size_t iteration = 0;
            bool isDone = false;

            while (!isDone)
            {
                auto context = service.GetContext();
                auto& hashTable = context[hashTableIndex];

                for (std::uint32_t j = 0; !isDone && j < options.m_numIterationsPerGetContext; ++j)
                {
                    const auto index = startIndex + iteration;
                    auto key = dataGenerator->GetKey(index);
                    auto val = dataGenerator->GetValue(index);

                    hashTable.Add(key, val);

                    isDone = (++iteration == info.m_dataSetSize);
                }
            }

            info.m_totalTime = totalTimer.GetElapsedTime();
            overallTimer.End();
        });
    }

    {
        std::unique_lock<std::mutex> lock(mutex);
        isReady = true;
    }

    // Now, start the benchmarking for all threads.
    cv.notify_all();

    for (auto& info : allInfo)
    {
        info.m_thread.join();
    }

    PrintHashTableCounters(service.GetContext()[hashTableIndex].GetPerfData());

    printf("Result:\n");
    printf("            |            Total             |               |\n");
    printf("            |  micros/op  |  microseconds  |  DataSetSize  |\n");
    printf(" -----------------------------------------------------------\n");

    for (std::size_t i = 0; i < allInfo.size(); ++i)
    {
        const auto& info = allInfo[i];

        printf(" Thread #%llu  | %11.3f | %14llu | %13llu |\n",
            (i + 1),
            static_cast<double>(info.m_totalTime.count()) / info.m_dataSetSize,
            info.m_totalTime.count(),
            info.m_dataSetSize);
    }
    printf(" -----------------------------------------------------------\n");

    printf(" Overall    | %11.3f | %14llu | %13llu |\n",
        static_cast<double>(overallTimer.GetElapsedTime().count()) / options.m_dataSetSize,
        overallTimer.GetElapsedTime().count(),
        options.m_dataSetSize);

    if (options.m_numThreads == 1)
    {
        auto& perfData = service.GetContext()[hashTableIndex].GetPerfData();
        std::uint64_t totalBytes = perfData.Get(L4::HashTablePerfCounter::TotalKeySize)
            + perfData.Get(L4::HashTablePerfCounter::TotalValueSize);

        auto& info = allInfo[0];

        double opsPerSec = static_cast<double>(info.m_dataSetSize) / info.m_totalTime.count() * 1000000.0;
        double MBPerSec = static_cast<double>(totalBytes) / info.m_totalTime.count();
        printf("  %10.3f ops/sec  %10.3f MB/sec\n", opsPerSec, MBPerSec);
    }
}


CommandLineOptions Parse(int argc, char** argv)
{
    namespace po = boost::program_options;

    po::options_description general("General options");
    general.add_options()
        ("help", "produce a help message")
        ("help-module", po::value<std::string>(),
            "produce a help for the following modules:\n"
            "  write-perf\n"
            "  overwrite-perf\n"
            "  read-perf\n"
            "  cache-read-perf\n"
            "  cache-write-perf\n")
        ("module", po::value<std::string>(),
            "Runs the given module");

    po::options_description benchmarkOptions("Benchmark options.");
    benchmarkOptions.add_options()
        ("dataSetSize", po::value<std::size_t>()->default_value(CommandLineOptions::c_defaultDataSetSize), "data set size")
        ("numBuckets", po::value<std::uint32_t>()->default_value(CommandLineOptions::c_defaultNumBuckets), "number of buckets")
        ("keySize", po::value<std::uint16_t>()->default_value(CommandLineOptions::c_defaultKeySize), "key size in bytes")
        ("valueSize", po::value<std::uint32_t>()->default_value(CommandLineOptions::c_defaultValueSize), "value size in bytes")
        ("randomizeValueSize", "randomize value size")
        ("numIterationsPerGetContext", po::value<std::uint32_t>()->default_value(CommandLineOptions::c_defaultNumIterationsPerGetContext), "number of iterations per GetContext()")
        ("numThreads", po::value<std::uint16_t>()->default_value(CommandLineOptions::c_defaultNumThreads), "number of threads to create")
        ("epochProcessingInterval", po::value<std::uint32_t>()->default_value(CommandLineOptions::c_defaultEpochProcessingIntervalInMilli), "epoch processing interval (ms)")
        ("numActionsQueue", po::value<std::uint8_t>()->default_value(CommandLineOptions::c_defaultNumActionsQueue), "number of actions queue")
        ("recordTimeToLive", po::value<std::uint32_t>()->default_value(CommandLineOptions::c_defaultRecordTimeToLiveInSeconds), "record time to live (s)")
        ("cacheSize", po::value<std::uint64_t>()->default_value(CommandLineOptions::c_defaultCacheSizeInBytes), "cache size in bytes")
        ("forceTimeBasedEviction", po::value<bool>()->default_value(CommandLineOptions::c_defaultForceTimeBasedEviction), "force time based eviction");

    po::options_description all("Allowed options");
    all.add(general).add(benchmarkOptions);

    po::variables_map vm;
    po::store(po::parse_command_line(argc, argv, all), vm);
    po::notify(vm);

    CommandLineOptions options;

    if (vm.count("help"))
    {
        std::cout << all;
    }
    else if (vm.count("module"))
    {
        options.m_module = vm["module"].as<std::string>();

        if (vm.count("dataSetSize"))
        {
            options.m_dataSetSize = vm["dataSetSize"].as<std::size_t>();
        }
        if (vm.count("numBuckets"))
        {
            options.m_numBuckets = vm["numBuckets"].as<std::uint32_t>();
        }
        if (vm.count("keySize"))
        {
            options.m_keySize = vm["keySize"].as<std::uint16_t>();
        }
        if (vm.count("valueSize"))
        {
            options.m_valueSize = vm["valueSize"].as<std::uint32_t>();
        }
        if (vm.count("randomizeValueSize"))
        {
            options.m_randomizeValueSize = true;
        }
        if (vm.count("numIterationsPerGetContext"))
        {
            options.m_numIterationsPerGetContext = vm["numIterationsPerGetContext"].as<std::uint32_t>();
        }
        if (vm.count("numThreads"))
        {
            options.m_numThreads = vm["numThreads"].as<std::uint16_t>();
        }
        if (vm.count("epochProcessingInterval"))
        {
            options.m_epochProcessingIntervalInMilli = vm["epochProcessingInterval"].as<std::uint32_t>();
        }
        if (vm.count("numActionsQueue"))
        {
            options.m_numActionsQueue = vm["numActionsQueue"].as<std::uint8_t>();
        }
        if (vm.count("recordTimeToLive"))
        {
            options.m_recordTimeToLiveInSeconds = vm["recordTimeToLive"].as<std::uint32_t>();
        }
        if (vm.count("cacheSize"))
        {
            options.m_cacheSizeInBytes = vm["cacheSize"].as<std::uint64_t>();
        }
        if (vm.count("forceTimeBasedEviction"))
        {
            options.m_forceTimeBasedEviction = vm["forceTimeBasedEviction"].as<bool>();
        }
    }
    else
    {
        std::cout << all;
    }

    return options;
}


int main(int argc, char** argv)
{
    auto options = Parse(argc, argv);

    if (options.m_module.empty())
    {
        return 0;
    }

    std::srand(static_cast<unsigned int>(time(NULL)));

    PrintHardwareInfo();

    if (options.m_module == "write-perf"
        || options.m_module == "overwrite-perf"
        || options.m_module == "cache-write-perf")
    { 
        WritePerfTest(options);
    }
    else if (options.m_module == "read-perf"
        || options.m_module == "cache-read-perf")
    {
        ReadPerfTest(options);
    }
    else
    {
        std::cout << "Unknown module: " << options.m_module << std::endl;
    }

    return 0;
}

