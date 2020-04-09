#include <gtest/gtest.h>

#include <prism/wrapper.h>
#include "mugenassignmentevaluator.h"

class MugenAssignmentEvaluatorTest : public ::testing::Test {
protected:
	MemoryStack mMemoryStack;

	void SetUp() override {
		initMemoryHandler();
		mMemoryStack = createMemoryStack(1024 * 1024 * 3);
		setupDreamAssignmentReader(&mMemoryStack);
		setupDreamAssignmentEvaluator();
	}

	void TearDown() override {
		shutdownMemoryHandler();
	}
};

static const auto FLOAT_EPSILON = 1e-5f;

TEST_F(MugenAssignmentEvaluatorTest, LogicalNegation) {
	auto assignment = parseDreamMugenAssignmentFromString("!0");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("!!0");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("!4");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("!!4");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, BitwiseInversion) {
	auto assignment = parseDreamMugenAssignmentFromString("~0");
	ASSERT_EQ(0xFFFFFFFF, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("~~0");
	ASSERT_EQ(0, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("~15");
	ASSERT_EQ(0xFFFFFFF0, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("~~15");
	ASSERT_EQ(15, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, UnaryMinus) {
	auto assignment = parseDreamMugenAssignmentFromString("-5");
	ASSERT_EQ(-5, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("--5");
	ASSERT_EQ(5, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("-0.5");
	ASSERT_NEAR(-0.5, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
	assignment = parseDreamMugenAssignmentFromString("--0.5");
	ASSERT_NEAR(0.5, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
}

TEST_F(MugenAssignmentEvaluatorTest, Power) {
	auto assignment = parseDreamMugenAssignmentFromString("5 ** 2");
	ASSERT_EQ(25, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("-5 ** 2");
	ASSERT_EQ(25, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("5 ** 2 * 2");
	ASSERT_EQ(50, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("2 * 5 ** 2");
	ASSERT_EQ(50, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, Multiplication) {
	auto assignment = parseDreamMugenAssignmentFromString("2 * 2");
	ASSERT_EQ(4, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("0.5 * 0.5");
	ASSERT_NEAR(0.25, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
	assignment = parseDreamMugenAssignmentFromString("1 * 0.5");
	ASSERT_NEAR(0.5, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
	assignment = parseDreamMugenAssignmentFromString("0.5 * 2");
	ASSERT_NEAR(1.0, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
	assignment = parseDreamMugenAssignmentFromString("0.5 * 3 * 2.0");
	ASSERT_NEAR(3.0, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
}

TEST_F(MugenAssignmentEvaluatorTest, Division) {
	auto assignment = parseDreamMugenAssignmentFromString("4 / 2");
	ASSERT_EQ(2, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("3.0 / 0.5");
	ASSERT_NEAR(6.0, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
	assignment = parseDreamMugenAssignmentFromString("1 / 4.0");
	ASSERT_NEAR(0.25, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
	assignment = parseDreamMugenAssignmentFromString("0.5 / 4");
	ASSERT_NEAR(0.125, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
	assignment = parseDreamMugenAssignmentFromString("0.5 / 10 / 2.0");
	ASSERT_NEAR(0.025, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
}

TEST_F(MugenAssignmentEvaluatorTest, Modulo) {
	auto assignment = parseDreamMugenAssignmentFromString("4 % 4");
	ASSERT_EQ(0, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("3 % 5");
	ASSERT_EQ(3, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("6 % 4");
	ASSERT_EQ(2, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("3 % 8 % 3");
	ASSERT_EQ(0, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, Addition) {
	auto assignment = parseDreamMugenAssignmentFromString("1 + 1");
	ASSERT_EQ(2, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("0.5 + 0.5");
	ASSERT_NEAR(1.0, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
	assignment = parseDreamMugenAssignmentFromString("1 + 0.5");
	ASSERT_NEAR(1.5, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
	assignment = parseDreamMugenAssignmentFromString("0.5 + 1");
	ASSERT_NEAR(1.5, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
	assignment = parseDreamMugenAssignmentFromString("0.5 + 1 + 0.5");
	ASSERT_NEAR(2.0, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);

	std::string result;
	assignment = parseDreamMugenAssignmentFromString("f30 + 5");
	evaluateDreamAssignmentAndReturnAsString(result, &assignment, NULL);
	ASSERT_EQ("isinotherfilef 35", result);
	assignment = parseDreamMugenAssignmentFromString("s20 + 5");
	evaluateDreamAssignmentAndReturnAsString(result, &assignment, NULL);
	ASSERT_EQ("isinotherfiles 25", result);
}

TEST_F(MugenAssignmentEvaluatorTest, GreaterThan) {
	auto assignment = parseDreamMugenAssignmentFromString("1 > 2");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("5 > 3");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("7 > 7");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("4 > 7 > 0");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, GreaterThanEqual) {
	auto assignment = parseDreamMugenAssignmentFromString("1 >= 2");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("5 >= 3");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("7 >= 7");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("4 >= 7 >= 0");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, LessThan) {
	auto assignment = parseDreamMugenAssignmentFromString("1 < 2");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("5 < 3");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("7 < 7");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("4 < 7 < 1");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, LessThanEqual) {
	auto assignment = parseDreamMugenAssignmentFromString("1 <= 2");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("5 <= 3");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("7 <= 7");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("4 <= 7 <= 1");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, Comparison) {
	auto assignment = parseDreamMugenAssignmentFromString("1 = 2");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("5 = 5 = 1");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("5 = 5 = 5");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, Inequality) {
	auto assignment = parseDreamMugenAssignmentFromString("1 != 2");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("5 != 5 != 1");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("5 != 5 != 0");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, BitwiseAnd) {
	auto assignment = parseDreamMugenAssignmentFromString("1 & 2");
	ASSERT_EQ(0, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("15 & 7 & 3");
	ASSERT_EQ(3, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("15 & 15 & 15");
	ASSERT_EQ(15, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, BitwiseXor) {
	auto assignment = parseDreamMugenAssignmentFromString("7 ^ 3");
	ASSERT_EQ(4, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("8 ^ 7");
	ASSERT_EQ(15, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("0 ^ 0");
	ASSERT_EQ(0, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("15 ^ 15");
	ASSERT_EQ(0, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("15 ^ 15 ^ 7");
	ASSERT_EQ(7, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, BitwiseOr) {
	auto assignment = parseDreamMugenAssignmentFromString("15 | 3");
	ASSERT_EQ(15, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("3 | 4");
	ASSERT_EQ(7, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("0 | 0");
	ASSERT_EQ(0, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("15 | 15");
	ASSERT_EQ(15, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("1 | 2 | 4 | 8 | 16");
	ASSERT_EQ(31, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, Subtraction) {
	auto assignment = parseDreamMugenAssignmentFromString("2 - 1");
	ASSERT_EQ(1, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("0.5 - 0.5");
	ASSERT_NEAR(0.0, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
	assignment = parseDreamMugenAssignmentFromString("1 - 0.5");
	ASSERT_NEAR(0.5, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
	assignment = parseDreamMugenAssignmentFromString("0.5 - 1");
	ASSERT_NEAR(-0.5, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
	assignment = parseDreamMugenAssignmentFromString("0.5 - 1 - 0.5");
	ASSERT_NEAR(-1.0, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
}

TEST_F(MugenAssignmentEvaluatorTest, LogicalAnd) {
	auto assignment = parseDreamMugenAssignmentFromString("1 && 1");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("1 && 0");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("0 && 1");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("0 && 0");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));

	assignment = parseDreamMugenAssignmentFromString("1 && 0 && 1");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("1 && 1 && 1");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));

	assignment = parseDreamMugenAssignmentFromString("1 && 1 && \"StringNearEndDoesNotCauseIssues\"");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, LogicalOr) {
	auto assignment = parseDreamMugenAssignmentFromString("1 || 1");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("1 || 0");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("0 || 1");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("0 || 0");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));

	assignment = parseDreamMugenAssignmentFromString("1 || 0 || 1");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("0 || 0 || 0");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, LogicalXor) {
	auto assignment = parseDreamMugenAssignmentFromString("1 ^^ 1");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("1 ^^ 0");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("0 ^^ 1");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("0 ^^ 0");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));

	assignment = parseDreamMugenAssignmentFromString("1 ^^ 0 ^^ 1 ^^ 1");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("1 ^^ 1 ^^ 1");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("1 ^^ 1 ^^ 1 ^^ 1");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, SameLevelPrecedence) {
	auto assignment = parseDreamMugenAssignmentFromString("4 + 2 - 3 + 5");
	ASSERT_EQ(8, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("2.0 * 3.0 / 10.0 * 4.0 / 2.0");
	ASSERT_NEAR(1.2, evaluateDreamAssignmentAndReturnAsFloat(&assignment, NULL), FLOAT_EPSILON);
	assignment = parseDreamMugenAssignmentFromString("4 * 8 / 2 * 10 / 5 * 10 % 7");
	ASSERT_EQ(5, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("4 * 8 / 2 * 10 % 7 * 10 % 7");
	ASSERT_EQ(4, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("-!!~1");
	ASSERT_EQ(-1, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("5 > 6 >= 1 < 0 <= 0");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("5 != 6 = 1 != 0");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, DifferentLevelPrecedence) {
	auto assignment = parseDreamMugenAssignmentFromString("-4 ** 2");
	ASSERT_EQ(16, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));

	assignment = parseDreamMugenAssignmentFromString("4 ** 2 * 2");
	ASSERT_EQ(32, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("2 * 4 ** 2");
	ASSERT_EQ(32, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));

	assignment = parseDreamMugenAssignmentFromString("5 + 2 * 4 + 3 * 6 - 2 * 8 - 5 * 5");
	ASSERT_EQ(-10, evaluateDreamAssignmentAndReturnAsInteger(&assignment, NULL));

	assignment = parseDreamMugenAssignmentFromString("5 + 2 >= 5 + 4");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
	assignment = parseDreamMugenAssignmentFromString("5 + 5 >= 5 + 4");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));

	assignment = parseDreamMugenAssignmentFromString("5 >= 5 & 3 <= 2");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));

	assignment = parseDreamMugenAssignmentFromString("1 ^ 4 & 2 ^ 1 ^ 1 & 1");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));

	assignment = parseDreamMugenAssignmentFromString("1 ^ 1 ^ 1 ^ 1 | 0 ^ 0");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));

	assignment = parseDreamMugenAssignmentFromString("1 && 1 | 0 && 1");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));

	assignment = parseDreamMugenAssignmentFromString("1 && 1 ^^ 1 && 0");
	ASSERT_TRUE(evaluateDreamAssignment(&assignment, NULL));

	assignment = parseDreamMugenAssignmentFromString("0 || 1 ^^ 1 || 0");
	ASSERT_FALSE(evaluateDreamAssignment(&assignment, NULL));
}

TEST_F(MugenAssignmentEvaluatorTest, MemoryLeaks) {
	setupDreamAssignmentReader(NULL);
	const auto prevAllocations = getAllocatedMemoryBlockAmount();
	auto assignment = parseDreamMugenAssignmentFromString("1 + 1 / 2 * 4 , 5 * (3 - 4)");
	ASSERT_NE(prevAllocations, getAllocatedMemoryBlockAmount());
	destroyDreamMugenAssignment(assignment);
	ASSERT_EQ(prevAllocations, getAllocatedMemoryBlockAmount());
}

TEST_F(MugenAssignmentEvaluatorTest, OperatorVector) {
	auto assignment = parseDreamMugenAssignmentFromString("animelem = 9,=0");
	ASSERT_EQ(assignment->mType, MUGEN_ASSIGNMENT_TYPE_COMPARISON);
	DreamMugenDependOnTwoAssignment* comparison = (DreamMugenDependOnTwoAssignment*)assignment;
	auto vectorAssignment = comparison->b;
	ASSERT_EQ(vectorAssignment->mType, MUGEN_ASSIGNMENT_TYPE_VECTOR);
	DreamMugenDependOnTwoAssignment* vector = (DreamMugenDependOnTwoAssignment*)vectorAssignment;
	ASSERT_EQ(vector->b->mType, MUGEN_ASSIGNMENT_TYPE_OPERATOR_ARGUMENT);
}

TEST_F(MugenAssignmentEvaluatorTest, AnimElemTimeArray) {
	auto assignment = parseDreamMugenAssignmentFromString("animelemtime(2)=-1");
	ASSERT_EQ(assignment->mType, MUGEN_ASSIGNMENT_TYPE_COMPARISON);
	DreamMugenDependOnTwoAssignment* comparison = (DreamMugenDependOnTwoAssignment*)assignment;
	ASSERT_EQ(comparison->a->mType, MUGEN_ASSIGNMENT_TYPE_ARRAY);
	ASSERT_EQ(comparison->b->mType, MUGEN_ASSIGNMENT_TYPE_UNARY_MINUS);
}