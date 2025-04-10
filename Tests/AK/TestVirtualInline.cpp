/*
 * Copyright (c) 2018-2023, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/VirtualInline.h>
#include <LibTest/Macros.h>
#include <LibTest/TestCase.h>

#include <AK/ByteString.h>
#include <AK/NonnullOwnPtr.h>
#include <AK/OwnPtr.h>
#include <AK/String.h>

static bool has_been_destructed = false;

class Base {
public:
    virtual ~Base() = default;
    virtual void do_work() { a = b; }
    int a;
    int b;
    int c;
};

class Derived : public Base {
public:
    virtual ~Derived() = default;
    virtual void do_work() override { a = c; }
    int d;
    int e { 0 };
};

constexpr size_t size = sizeof(Derived);

TEST_CASE(inheritance)
{
    VirtualInline<Base, size> base;

    base->a = 1;
    base->b = 10;
    base->c = 2;
    base->do_work();
    EXPECT_EQ(base->a, 10);

    // now put a derived class instance in there, which should behave as normal
    base = Derived();
    base->a = 1;
    base->b = 10;
    base->c = 2;
    base->do_work();
    EXPECT_EQ(base->a, 2);

    Derived not_wrapped {};
    VirtualInline<Derived, size> copy_constructed { not_wrapped };
    VirtualInline<Derived, size> move_constructed { move(not_wrapped) };
    EXPECT_EQ(move_constructed->a, 0);
    move_constructed->do_work();
    VirtualInline<Derived, size> double_move { move(move_constructed) };
    double_move->do_work();
    VirtualInline<Derived, size> double_copy { copy_constructed };
    double_copy->do_work();

    base->a = 4;
    VirtualInline<Derived, size> derived { base };
    EXPECT_EQ(derived->a, base->a);
    EXPECT_EQ(derived->c, base->c);
    EXPECT_EQ(derived->e, 0);
    VirtualInline<Derived, size> derived2 = base;
    EXPECT_EQ(derived2->a, base->a);
    EXPECT_EQ(derived2->c, base->c);
    EXPECT_EQ(derived2->e, 0);

    VirtualInline<Derived, size> moved_derived = move(base);
    EXPECT_EQ(moved_derived->a, derived->a);
    EXPECT_EQ(moved_derived->c, derived->c);
    EXPECT_EQ(moved_derived->e, 0);

    derived->a = 30;
    derived = derived2;
    derived->do_work();
    EXPECT_NE(derived->a, derived2->a);
    EXPECT_EQ(derived->c, derived2->c);
    derived = move(derived2);
    derived->do_work();
    base->a = -2;
    base = derived;
    base->do_work();
    EXPECT_NE(derived->a, base->a);
    EXPECT_EQ(derived->c, base->c);
    base->a = -2;
    base = move(moved_derived);
    base->do_work();
    EXPECT_NE(base->a, derived->a);
    EXPECT_EQ(base->c, derived->c);
}

class Destructible {
public:
    ~Destructible() { has_been_destructed = true; }
};

class VirtuallyDestructible {
public:
    virtual ~VirtuallyDestructible() = default;
};

class VirtuallyDestructibleChild : public VirtuallyDestructible {
public:
    virtual ~VirtuallyDestructibleChild() { has_been_destructed = true; }
};

TEST_CASE(destruction)
{
    EXPECT_EQ(has_been_destructed, false);
    {
        VirtualInline<Destructible, sizeof(Destructible)> destructible;
    }
    EXPECT_EQ(has_been_destructed, true);

    has_been_destructed = false;
    {
        VirtualInline<VirtuallyDestructible, sizeof(VirtuallyDestructible)> destructible;
        destructible = VirtuallyDestructibleChild();
    }
    EXPECT_EQ(has_been_destructed, true);

    has_been_destructed = false;
    {
        VirtualInline<VirtuallyDestructible, sizeof(VirtuallyDestructible)> destructible { VirtuallyDestructibleChild() };
        destructible = VirtuallyDestructible();
        EXPECT_EQ(has_been_destructed, true);
    }
}

struct Foo { };

template<>
struct AK::Formatter<Foo> : Formatter<StringView> {
    ErrorOr<void> format(FormatBuilder& builder, Foo const&)
    {
        return Formatter<StringView>::format(builder, ":^)"sv);
    }
};

TEST_CASE(formatter)
{
    auto foo = VirtualInline<Foo, sizeof(Foo)>();
    EXPECT_EQ(MUST(String::formatted("{}", foo)), ":^)"sv);
}
