#pragma once
/* Minimal Google Test compatible shim for sandboxed builds without
 * gtest. Supports: TEST, TEST_F, ASSERT_/EXPECT_ EQ/NE/TRUE/FALSE/NEAR,
 * ::testing::Test fixtures with SetUp/TearDown, InitGoogleTest with
 * --gtest_filter=Suite.Name (exact or Suite.* patterns), RUN_ALL_TESTS.
 * Swap for real gtest by removing this include dir from the build. */

#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <string>
#include <vector>
#include <functional>

namespace testing {

class Test {
public:
	virtual ~Test() {}
	virtual void SetUp() {}
	virtual void TearDown() {}
	virtual void TestBody() = 0;
	void Run() { SetUp(); TestBody(); TearDown(); }
};

struct TestInfo {
	const char* mSuite;
	const char* mName;
	std::function<Test*()> mFactory;
};

inline std::vector<TestInfo>& registry() {
	static std::vector<TestInfo> r;
	return r;
}
inline std::string& filterPattern() {
	static std::string f = "*";
	return f;
}
inline int& failureCount() { static int c = 0; return c; }
inline bool& currentTestFailed() { static bool f = false; return f; }

inline int registerTest(const char* tSuite, const char* tName, std::function<Test*()> tFactory) {
	registry().push_back({ tSuite, tName, tFactory });
	return (int)registry().size();
}

inline bool matchesFilter(const std::string& tFull) {
	const auto& f = filterPattern();
	if (f == "*" || f.empty()) return true;
	auto star = f.find('*');
	if (star == std::string::npos) return tFull == f;
	return tFull.compare(0, star, f, 0, star) == 0;
}

inline void InitGoogleTest(int* argc, char** argv) {
	static const char* prefix = "--gtest_filter=";
	for (int i = 1; i < *argc; i++) {
		if (!strncmp(argv[i], prefix, strlen(prefix))) {
			filterPattern() = argv[i] + strlen(prefix);
		}
	}
}

} // namespace testing

inline int RUN_ALL_TESTS() {
	int run = 0;
	for (const auto& t : testing::registry()) {
		std::string full = std::string(t.mSuite) + "." + t.mName;
		if (!testing::matchesFilter(full)) continue;
		printf("[ RUN      ] %s\n", full.c_str());
		fflush(stdout);
		testing::currentTestFailed() = false;
		testing::Test* test = t.mFactory();
		test->Run();
		delete test;
		run++;
		if (testing::currentTestFailed()) {
			printf("[  FAILED  ] %s\n", full.c_str());
		} else {
			printf("[       OK ] %s\n", full.c_str());
		}
		fflush(stdout);
	}
	printf("[==========] %d tests ran, %d failures.\n", run, testing::failureCount());
	return testing::failureCount() ? 1 : 0;
}

#define GTEST_CLASS_NAME_(suite, name) suite##_##name##_Test

#define GTEST_TEST_(suite, name, parent) \
	class GTEST_CLASS_NAME_(suite, name) : public parent { \
	public: \
		void TestBody() override; \
		static int sRegistered; \
	}; \
	int GTEST_CLASS_NAME_(suite, name)::sRegistered = ::testing::registerTest( \
		#suite, #name, []() -> ::testing::Test* { return new GTEST_CLASS_NAME_(suite, name)(); }); \
	void GTEST_CLASS_NAME_(suite, name)::TestBody()

#define TEST(suite, name) GTEST_TEST_(suite, name, ::testing::Test)
#define TEST_F(fixture, name) GTEST_TEST_(fixture, name, fixture)

#define GTEST_FAIL_(msg) do { \
	printf("FAILURE %s:%d: %s\n", __FILE__, __LINE__, msg); \
	fflush(stdout); \
	testing::failureCount()++; \
	testing::currentTestFailed() = true; \
	return; \
} while (0)

#define ASSERT_TRUE(cond) do { if (!(cond)) GTEST_FAIL_("expected true: " #cond); } while (0)
#define ASSERT_FALSE(cond) do { if (cond) GTEST_FAIL_("expected false: " #cond); } while (0)
#define ASSERT_EQ(a, b) do { if (!((a) == (b))) GTEST_FAIL_("expected equality of " #a " and " #b); } while (0)
#define ASSERT_NE(a, b) do { if ((a) == (b)) GTEST_FAIL_("expected inequality of " #a " and " #b); } while (0)
#define ASSERT_NEAR(a, b, eps) do { if (std::fabs(double(a) - double(b)) > double(eps)) GTEST_FAIL_("expected near: " #a " and " #b); } while (0)
#define EXPECT_TRUE ASSERT_TRUE
#define EXPECT_FALSE ASSERT_FALSE
#define EXPECT_EQ ASSERT_EQ
#define EXPECT_NE ASSERT_NE
#define EXPECT_NEAR ASSERT_NEAR
