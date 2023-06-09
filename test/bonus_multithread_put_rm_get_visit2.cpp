#include <iostream>

#include "engine.h"
#include "gflags/gflags.h"
#include "glog/logging.h"
#include "util/bench.h"
#include "util/executor.h"

using namespace kvs;
using namespace bench;

DEFINE_uint64(test_nr, 20000, "The number of tests");
DEFINE_string(kvdir, kDefaultTestDir, "The KV Store data directory");
DEFINE_uint32(thread_nr, 8, "The number of threads to run");

int main(int argc, char *argv[])
{
    google::InitGoogleLogging(argv[0]);
    gflags::ParseCommandLineFlags(&argc, &argv, true);
    FLAGS_logtostderr = true;
    FLAGS_colorlogtostderr = true;

    LOG(INFO) << "constructing Engine...";

    auto engine = Engine::new_instance(FLAGS_kvdir, EngineOptions{});

    std::vector<std::thread> threads;
    std::vector<std::vector<TestCase>> test_cases;

    LOG(INFO) << "Generating data...";
    std::vector<std::shared_ptr<IRandAllocator>> allocators;
    for (size_t i = 0; i < FLAGS_thread_nr; ++i)
    {
        auto prefix = std::string(1, (char) ('a' + i));
        allocators.emplace_back(std::make_shared<PrefixAllocator>(prefix));
    }
    std::vector<bench::BenchGenerator> bench_generators;
    for (size_t i = 0; i < FLAGS_thread_nr; ++i)
    {
        bench::BenchGenerator g({{Op::kGet, 0.20, 2, 4, 0.2, 0},
                                 {Op::kPut, 0.5, 2, 4, 0.3, 0},
                                 {Op::kDelete, 0.20, 2, 4, 0.3, 0},
                                 {Op::kVisit, 0.1, 0, 0, 0, 0.5}},
                                allocators[i]);
        bench_generators.emplace_back(std::move(g));
    }
    for (size_t i = 0; i < FLAGS_thread_nr; ++i)
    {
        test_cases.emplace_back(
            bench_generators[i].rand_tests(FLAGS_test_nr, 0));
    }

    LOG(INFO) << "Starting tests...";
    for (size_t i = 0; i < FLAGS_thread_nr; ++i)
    {
        threads.emplace_back(
            [engine, &test_case = test_cases[i]]()
            {
                EngineValidator executor(engine);
                executor.validate_execute(test_case);
            });
    }

    for (auto &t : threads)
    {
        t.join();
    }

    LOG(INFO) << "PASS " << argv[0];

    return 0;
}