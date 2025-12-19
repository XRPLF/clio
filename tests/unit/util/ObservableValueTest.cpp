//------------------------------------------------------------------------------
/*
    This file is part of clio: https://github.com/XRPLF/clio
    Copyright (c) 2025, the clio developers.

    Permission to use, copy, modify, and distribute this software for any
    purpose with or without fee is hereby granted, provided that the above
    copyright notice and this permission notice appear in all copies.

    THE  SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
    WITH  REGARD  TO  THIS  SOFTWARE  INCLUDING  ALL  IMPLIED  WARRANTIES  OF
    MERCHANTABILITY  AND  FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
    ANY  SPECIAL,  DIRECT,  INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
    WHATSOEVER  RESULTING  FROM  LOSS  OF USE, DATA OR PROFITS, WHETHER IN AN
    ACTION  OF  CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
*/
//==============================================================================

#include "util/ObservableValue.hpp"

#include <boost/signals2/connection.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <concepts>
#include <map>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

using namespace testing;
using namespace util;

namespace {

struct TestStruct {
    int value = 0;
    std::string name;

    bool
    operator==(TestStruct const& other) const
    {
        return value == other.value && name == other.name;
    }

    bool
    operator!=(TestStruct const& other) const
    {
        return !(*this == other);
    }
};

}  // namespace

class ObservableValueTest : public ::testing::Test {};

TEST_F(ObservableValueTest, ConceptCompliance)
{
    static_assert(Observable<int>);
    static_assert(Observable<std::string>);
    static_assert(Observable<double>);
    static_assert(Observable<TestStruct>);
    static_assert(Observable<bool>);
    static_assert(Observable<char>);
    static_assert(Observable<float>);

    struct NonCopyable {
        int value = 0;
        NonCopyable() = default;
        NonCopyable(NonCopyable const&) = delete;
        NonCopyable(NonCopyable&&) = default;
        NonCopyable&
        operator=(NonCopyable const&) = delete;
        NonCopyable&
        operator=(NonCopyable&&) = default;
        bool
        operator==(NonCopyable const& other) const
        {
            return value == other.value;
        }
    };
    static_assert(!Observable<NonCopyable>);

    struct NonMovable {
        int value = 0;
        NonMovable() = default;
        NonMovable(NonMovable const&) = default;
        NonMovable(NonMovable&&) = delete;
        NonMovable&
        operator=(NonMovable const&) = default;
        NonMovable&
        operator=(NonMovable&&) = delete;
        bool
        operator==(NonMovable const& other) const
        {
            return value == other.value;
        }
    };
    static_assert(!Observable<NonMovable>);

    struct NonComparable {
        int value = 0;
        NonComparable() = default;
        NonComparable(NonComparable const&) = default;
        NonComparable(NonComparable&&) = default;
        NonComparable&
        operator=(NonComparable const&) = default;
        NonComparable&
        operator=(NonComparable&&) = default;
    };
    static_assert(!Observable<NonComparable>);

    struct NonDefaultInitializable {
        int value;
        NonDefaultInitializable() = delete;
        explicit NonDefaultInitializable(int v) : value(v)
        {
        }
        NonDefaultInitializable(NonDefaultInitializable const&) = default;
        NonDefaultInitializable(NonDefaultInitializable&&) = default;
        NonDefaultInitializable&
        operator=(NonDefaultInitializable const&) = default;
        NonDefaultInitializable&
        operator=(NonDefaultInitializable&&) = default;
        bool
        operator==(NonDefaultInitializable const& other) const
        {
            return value == other.value;
        }
    };

    static_assert(Observable<NonDefaultInitializable>);
    static_assert(!std::default_initializable<NonDefaultInitializable>);

    static_assert(Observable<std::vector<int>>);
    static_assert(Observable<std::map<int, int>>);
    static_assert(Observable<std::set<int>>);
    static_assert(Observable<std::pair<int, std::string>>);

    static_assert(std::default_initializable<int>);
    static_assert(std::default_initializable<std::string>);
    static_assert(std::default_initializable<std::vector<int>>);
    static_assert(std::default_initializable<TestStruct>);
}

TEST_F(ObservableValueTest, Construction)
{
    ObservableValue<int> const obs{42};

    EXPECT_EQ(static_cast<int>(obs), 42);
    EXPECT_EQ(obs.get(), 42);
    EXPECT_FALSE(obs.hasObservers());
}

TEST_F(ObservableValueTest, ConstructionWithDifferentTypes)
{
    ObservableValue<std::string> const obsStr{"hello"};
    EXPECT_EQ(obsStr.get(), "hello");

    ObservableValue<double> const obsDouble{3.14};
    EXPECT_DOUBLE_EQ(obsDouble.get(), 3.14);

    ObservableValue<bool> const obsBool{true};
    EXPECT_TRUE(obsBool.get());
}

TEST_F(ObservableValueTest, DefaultConstruction)
{
    ObservableValue<int> const obsInt;
    EXPECT_EQ(obsInt.get(), 0);

    ObservableValue<double> const obsDouble;
    EXPECT_DOUBLE_EQ(obsDouble.get(), 0.0);

    ObservableValue<bool> const obsBool;
    EXPECT_FALSE(obsBool.get());

    ObservableValue<char> const obsChar;
    EXPECT_EQ(obsChar.get(), '\0');

    EXPECT_FALSE(obsInt.hasObservers());
    EXPECT_FALSE(obsDouble.hasObservers());
    EXPECT_FALSE(obsBool.hasObservers());
    EXPECT_FALSE(obsChar.hasObservers());
}

TEST_F(ObservableValueTest, DefaultConstructionWithContainers)
{
    ObservableValue<std::string> const obsString;
    EXPECT_EQ(obsString.get(), "");
    EXPECT_TRUE(obsString.get().empty());

    ObservableValue<std::vector<int>> const obsVector;
    EXPECT_TRUE(obsVector.get().empty());
    EXPECT_EQ(obsVector.get().size(), 0);

    ObservableValue<std::set<int>> const obsSet;
    EXPECT_TRUE(obsSet.get().empty());
    EXPECT_EQ(obsSet.get().size(), 0);

    ObservableValue<std::map<int, std::string>> const obsMap;
    EXPECT_TRUE(obsMap.get().empty());
    EXPECT_EQ(obsMap.get().size(), 0);
}

TEST_F(ObservableValueTest, DefaultConstructionWithCustomType)
{
    ObservableValue<TestStruct> const obsStruct;
    EXPECT_EQ(obsStruct.get().value, 0);
    EXPECT_EQ(obsStruct.get().name, "");
}

TEST_F(ObservableValueTest, DefaultConstructionThenAssignment)
{
    ObservableValue<int> obs;
    EXPECT_EQ(obs.get(), 0);

    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;
    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(42));
    obs = 42;
    EXPECT_EQ(obs.get(), 42);

    obs = 42;  // Same value, should not notify

    EXPECT_CALL(mockObserver, Call(100));
    obs.set(100);
    EXPECT_EQ(obs.get(), 100);
}

TEST_F(ObservableValueTest, DefaultConstructionWithGuard)
{
    ObservableValue<std::string> obs;
    EXPECT_EQ(obs.get(), "");

    testing::StrictMock<testing::MockFunction<void(std::string const&)>> mockObserver;
    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call("modified through guard"));
    {
        auto guard = obs.operator->();
        std::string& ref = guard;
        ref = "modified through guard";
    }

    EXPECT_EQ(obs.get(), "modified through guard");
}

TEST_F(ObservableValueTest, DefaultConstructionNotificationBehavior)
{
    ObservableValue<int> obs;
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;
    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(1));
    obs = 1;

    EXPECT_CALL(mockObserver, Call(0));
    obs = 0;

    obs = 0;  // Same value, should not notify
}

TEST_F(ObservableValueTest, NonDefaultInitializableTypeWithParameterizedConstructor)
{
    struct NonDefaultInitializable {
        int value;
        NonDefaultInitializable() = delete;
        explicit NonDefaultInitializable(int v) : value(v)
        {
        }
        NonDefaultInitializable(NonDefaultInitializable const&) = default;
        NonDefaultInitializable(NonDefaultInitializable&&) = default;
        NonDefaultInitializable&
        operator=(NonDefaultInitializable const&) = default;
        NonDefaultInitializable&
        operator=(NonDefaultInitializable&&) = default;
        bool
        operator==(NonDefaultInitializable const& other) const
        {
            return value == other.value;
        }
    };

    ObservableValue<NonDefaultInitializable> obs{NonDefaultInitializable{42}};
    EXPECT_EQ(obs.get().value, 42);

    testing::StrictMock<testing::MockFunction<void(NonDefaultInitializable const&)>> mockObserver;
    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(testing::Field(&NonDefaultInitializable::value, 100)));
    obs = NonDefaultInitializable{100};
    EXPECT_EQ(obs.get().value, 100);
}

TEST_F(ObservableValueTest, MoveSemantics)
{
    ObservableValue<int> const obs1{100};

    ObservableValue<int> const obs2 = std::move(obs1);
    EXPECT_EQ(obs2.get(), 100);

    ObservableValue<int> obs3{200};
    obs3 = std::move(obs2);
    EXPECT_EQ(obs3.get(), 100);
}

TEST_F(ObservableValueTest, CopyOperationsDeleted)
{
    static_assert(!std::is_copy_constructible_v<ObservableValue<int>>);
    static_assert(!std::is_copy_assignable_v<ObservableValue<int>>);
}

TEST_F(ObservableValueTest, AssignmentOperator)
{
    ObservableValue<int> obs{10};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(20));
    obs = 20;
    EXPECT_EQ(obs.get(), 20);

    obs = 20;  // Same value, should not notify
    EXPECT_EQ(obs.get(), 20);
}

TEST_F(ObservableValueTest, SetMethod)
{
    ObservableValue<int> obs{5};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(15));
    obs.set(15);
    EXPECT_EQ(obs.get(), 15);

    obs.set(15);  // Same value, should not notify
    EXPECT_EQ(obs.get(), 15);
}

TEST_F(ObservableValueTest, ObserverManagement)
{
    ObservableValue<int> obs{0};

    EXPECT_FALSE(obs.hasObservers());

    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver1;
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver2;

    auto conn1 = obs.observe(mockObserver1.AsStdFunction());
    EXPECT_TRUE(obs.hasObservers());

    auto conn2 = obs.observe(mockObserver2.AsStdFunction());
    EXPECT_TRUE(obs.hasObservers());

    EXPECT_CALL(mockObserver1, Call(42));
    EXPECT_CALL(mockObserver2, Call(42));
    obs = 42;

    conn1.disconnect();
    EXPECT_CALL(mockObserver2, Call(100));
    obs = 100;

    conn2.disconnect();
    EXPECT_FALSE(obs.hasObservers());

    obs = 200;  // No observers, no calls expected
}

TEST_F(ObservableValueTest, ObservableGuardBasicUsage)
{
    ObservableValue<int> obs{10};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(25));
    {
        auto guard = obs.operator->();
        int& ref = guard;
        ref = 25;
    }

    EXPECT_EQ(obs.get(), 25);
}

TEST_F(ObservableValueTest, ObservableGuardNoChangeNoNotification)
{
    ObservableValue<int> obs{50};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    // No EXPECT_CALL since no notification should occur
    {
        auto guard = obs.operator->();
        int& ref = guard;
        ref = 100;
        ref = 50;  // Back to original value
    }

    EXPECT_EQ(obs.get(), 50);
}

TEST_F(ObservableValueTest, ObservableGuardMultipleChanges)
{
    ObservableValue<int> obs{1};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(2));
    {
        auto guard = obs.operator->();
        int& ref = guard;
        ref = 2;
    }

    EXPECT_CALL(mockObserver, Call(3));
    {
        auto guard = obs.operator->();
        int& ref = guard;
        ref = 3;
    }

    EXPECT_EQ(obs.get(), 3);
}

TEST_F(ObservableValueTest, ComplexTypeObservation)
{
    TestStruct const initial{.value = 42, .name = "test"};
    ObservableValue<TestStruct> obs{initial};

    testing::StrictMock<testing::MockFunction<void(TestStruct const&)>> mockObserver;
    auto connection = obs.observe(mockObserver.AsStdFunction());

    TestStruct const newValue{.value = 100, .name = "changed"};
    EXPECT_CALL(
        mockObserver,
        Call(testing::AllOf(testing::Field(&TestStruct::value, 100), testing::Field(&TestStruct::name, "changed")))
    );
    obs = newValue;
}

TEST_F(ObservableValueTest, ComplexTypeGuardModification)
{
    TestStruct const initial{.value = 10, .name = "initial"};
    ObservableValue<TestStruct> obs{initial};

    testing::StrictMock<testing::MockFunction<void(TestStruct const&)>> mockObserver;
    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(
        mockObserver,
        Call(testing::AllOf(testing::Field(&TestStruct::value, 20), testing::Field(&TestStruct::name, "modified")))
    );
    {
        auto guard = obs.operator->();
        TestStruct& ref = guard;
        ref.value = 20;
        ref.name = "modified";
    }

    EXPECT_EQ(obs.get().value, 20);
    EXPECT_EQ(obs.get().name, "modified");
}

TEST_F(ObservableValueTest, StringObservation)
{
    ObservableValue<std::string> obs{"initial"};
    testing::StrictMock<testing::MockFunction<void(std::string const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call("changed"));
    obs = "changed";

    EXPECT_CALL(mockObserver, Call("set_method"));
    obs.set("set_method");

    obs = "set_method";  // Same value, should not notify
}

TEST_F(ObservableValueTest, MultipleObserversWithDifferentLifetimes)
{
    ObservableValue<int> obs{0};

    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver1;
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver2;
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver3;

    auto conn1 = obs.observe(mockObserver1.AsStdFunction());

    EXPECT_CALL(mockObserver1, Call(1));
    obs = 1;

    auto conn2 = obs.observe(mockObserver2.AsStdFunction());
    EXPECT_CALL(mockObserver1, Call(2));
    EXPECT_CALL(mockObserver2, Call(2));
    obs = 2;

    conn1.disconnect();
    auto conn3 = obs.observe(mockObserver3.AsStdFunction());
    EXPECT_CALL(mockObserver2, Call(3));
    EXPECT_CALL(mockObserver3, Call(3));
    obs = 3;
}

TEST_F(ObservableValueTest, NoNotificationWhenNoObservers)
{
    ObservableValue<int> obs{0};

    obs = 1;
    obs.set(2);

    {
        auto guard = obs.operator->();
        int& ref = guard;
        ref = 3;
    }

    EXPECT_EQ(obs.get(), 3);
    EXPECT_FALSE(obs.hasObservers());
}

TEST_F(ObservableValueTest, ManyObservers)
{
    ObservableValue<int> obs{0};

    std::vector<std::unique_ptr<testing::StrictMock<testing::MockFunction<void(int const&)>>>> mockObservers;
    std::vector<boost::signals2::connection> connections;

    constexpr int kNUM_OBSERVERS = 100;
    for (int i = 0; i < kNUM_OBSERVERS; ++i) {
        mockObservers.push_back(std::make_unique<testing::StrictMock<testing::MockFunction<void(int const&)>>>());
        connections.push_back(obs.observe(mockObservers.back()->AsStdFunction()));
    }

    EXPECT_TRUE(obs.hasObservers());

    for (auto const& mockObserver : mockObservers) {
        EXPECT_CALL(*mockObserver, Call(42));
    }
    obs = 42;

    for (auto& conn : connections) {
        conn.disconnect();
    }

    EXPECT_FALSE(obs.hasObservers());
}

TEST_F(ObservableValueTest, TypeConversions)
{
    ObservableValue<double> obs{1.0};

    testing::StrictMock<testing::MockFunction<void(double const&)>> mockObserver;
    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(testing::DoubleEq(2.0)));
    obs = 2;

    EXPECT_CALL(mockObserver, Call(testing::DoubleEq(3.14)));
    obs = 3.14;

    EXPECT_CALL(mockObserver, Call(testing::DoubleEq(4.0)));
    obs = static_cast<double>(4.0f);
}

TEST_F(ObservableValueTest, EnhancedConceptRequirements)
{
    struct ComplexObservable {
        std::string name;
        int value{};
        std::vector<int> data;

        ComplexObservable() = default;
        ComplexObservable(std::string n, int v, std::vector<int> d) : name(std::move(n)), value(v), data(std::move(d))
        {
        }
        ComplexObservable(ComplexObservable const& other) = default;
        ComplexObservable(ComplexObservable&& other) noexcept = default;

        ComplexObservable&
        operator=(ComplexObservable&& other) noexcept
        {
            if (this != &other) {
                name = std::move(other.name);
                value = other.value;
                data = std::move(other.data);
            }
            return *this;
        }

        bool
        operator==(ComplexObservable const& other) const
        {
            return name == other.name && value == other.value && data == other.data;
        }

        ComplexObservable&
        operator=(ComplexObservable const& other)
        {
            if (this != &other) {
                name = other.name;
                value = other.value;
                data = other.data;
            }
            return *this;
        }
    };

    static_assert(Observable<ComplexObservable>);

    ComplexObservable initial{"test", 42, {1, 2, 3}};
    ObservableValue<ComplexObservable> obs{std::move(initial)};

    testing::StrictMock<testing::MockFunction<void(ComplexObservable const&)>> mockObserver;
    auto connection = obs.observe(mockObserver.AsStdFunction());

    ComplexObservable const newValue{"changed", 100, {4, 5, 6}};
    EXPECT_CALL(
        mockObserver,
        Call(
            testing::AllOf(
                testing::Field(&ComplexObservable::name, "changed"),
                testing::Field(&ComplexObservable::value, 100),
                testing::Field(&ComplexObservable::data, std::vector<int>({4, 5, 6}))
            )
        )
    );
    obs = newValue;

    ComplexObservable const sameValue{"changed", 100, {4, 5, 6}};
    obs = sameValue;  // Same value, should not notify
}

TEST_F(ObservableValueTest, ExceptionInObserver)
{
    ObservableValue<int> obs{0};

    testing::StrictMock<testing::MockFunction<void(int const&)>> goodMockObserver;
    auto goodConnection = obs.observe(goodMockObserver.AsStdFunction());

    auto throwingConnection = obs.observe([](int const&) { throw std::runtime_error("Observer exception"); });

    EXPECT_CALL(goodMockObserver, Call(42));
    EXPECT_THROW(obs = 42, std::runtime_error);

    // Value is still updated even when observers throw
    EXPECT_EQ(obs.get(), 42);
}

TEST_F(ObservableValueTest, GuardExceptionSafety)
{
    ObservableValue<int> obs{10};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(20));
    try {
        auto guard = obs.operator->();
        int& ref = guard;
        ref = 20;
        throw std::runtime_error("Test exception");
    } catch (...) {
        [[maybe_unused]] auto nothing = true;
    }

    EXPECT_EQ(obs.get(), 20);
}

TEST_F(ObservableValueTest, ComprehensiveIntegrationTest)
{
    ObservableValue<std::string> obs{"start"};

    testing::StrictMock<testing::MockFunction<void(std::string const&)>> mockObserver1;
    testing::StrictMock<testing::MockFunction<void(std::string const&)>> mockObserver2;
    auto conn1 = obs.observe(mockObserver1.AsStdFunction());
    auto conn2 = obs.observe(mockObserver2.AsStdFunction());

    EXPECT_CALL(mockObserver1, Call("first"));
    EXPECT_CALL(mockObserver2, Call("first"));
    obs = "first";

    EXPECT_CALL(mockObserver1, Call("second"));
    EXPECT_CALL(mockObserver2, Call("second"));
    obs.set("second");

    obs = "second";  // Same value, should not notify

    EXPECT_CALL(mockObserver1, Call("third"));
    EXPECT_CALL(mockObserver2, Call("third"));
    {
        auto guard = obs.operator->();
        std::string& ref = guard;
        ref = "third";
    }

    conn1.disconnect();
    EXPECT_CALL(mockObserver2, Call("fourth"));
    obs = "fourth";

    EXPECT_EQ(obs.get(), "fourth");
    EXPECT_TRUE(obs.hasObservers());

    conn2.disconnect();
    EXPECT_FALSE(obs.hasObservers());
}

TEST_F(ObservableValueTest, RegularConnectionPersistsAfterDestruction)
{
    ObservableValue<int> obs{0};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    {
        auto connection = obs.observe(mockObserver.AsStdFunction());
        EXPECT_CALL(mockObserver, Call(1));
        obs = 1;
    }

    EXPECT_CALL(mockObserver, Call(2));
    obs = 2;

    EXPECT_TRUE(obs.hasObservers());
}

TEST_F(ObservableValueTest, ScopedConnectionDisconnectsOnDestruction)
{
    ObservableValue<int> obs{0};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    {
        boost::signals2::scoped_connection const scoped = obs.observe(mockObserver.AsStdFunction());
        EXPECT_CALL(mockObserver, Call(1));
        obs = 1;
        EXPECT_TRUE(obs.hasObservers());
    }

    obs = 2;  // No call expected since connection was destroyed
    EXPECT_FALSE(obs.hasObservers());
}

TEST_F(ObservableValueTest, ManualDisconnectWithRegularConnection)
{
    ObservableValue<int> obs{0};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(1));
    obs = 1;
    EXPECT_TRUE(obs.hasObservers());

    connection.disconnect();

    obs = 2;  // No call expected since connection was disconnected
    EXPECT_FALSE(obs.hasObservers());
}

TEST_F(ObservableValueTest, ScopedConnectionCanBeDisconnectedManually)
{
    ObservableValue<int> obs{0};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    boost::signals2::scoped_connection const scoped = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(1));
    obs = 1;
    EXPECT_TRUE(obs.hasObservers());

    scoped.disconnect();

    obs = 2;  // No call expected since connection was disconnected
    EXPECT_FALSE(obs.hasObservers());
}

TEST_F(ObservableValueTest, MixedConnectionTypes)
{
    ObservableValue<int> obs{0};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver1;
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver2;
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver3;

    auto regularConn = obs.observe(mockObserver1.AsStdFunction());

    {
        boost::signals2::scoped_connection const scoped1 = obs.observe(mockObserver2.AsStdFunction());
        boost::signals2::scoped_connection const scoped2 = obs.observe(mockObserver3.AsStdFunction());

        EXPECT_CALL(mockObserver1, Call(1));
        EXPECT_CALL(mockObserver2, Call(1));
        EXPECT_CALL(mockObserver3, Call(1));
        obs = 1;
        EXPECT_TRUE(obs.hasObservers());
    }

    EXPECT_CALL(mockObserver1, Call(2));
    obs = 2;  // Only mockObserver1 should be called since scoped connections were destroyed
    EXPECT_TRUE(obs.hasObservers());

    regularConn.disconnect();
    EXPECT_FALSE(obs.hasObservers());
}

TEST_F(ObservableValueTest, ForceNotify)
{
    ObservableValue<int> obs{42};
    testing::StrictMock<testing::MockFunction<void(int const&)>> mockObserver;

    obs.forceNotify();

    auto connection = obs.observe(mockObserver.AsStdFunction());

    EXPECT_CALL(mockObserver, Call(42));
    obs.forceNotify();

    EXPECT_CALL(mockObserver, Call(42));
    obs.forceNotify();

    EXPECT_CALL(mockObserver, Call(100));
    obs.set(100);
    EXPECT_CALL(mockObserver, Call(100));
    obs.forceNotify();

    EXPECT_CALL(mockObserver, Call(100)).Times(3);
    obs.forceNotify();
    obs.forceNotify();
    obs.forceNotify();
}
