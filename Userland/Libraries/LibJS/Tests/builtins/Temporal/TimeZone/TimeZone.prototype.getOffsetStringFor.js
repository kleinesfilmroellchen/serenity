describe("correct behavior", () => {
    test("length is 1", () => {
        expect(Temporal.TimeZone.prototype.getOffsetStringFor).toHaveLength(1);
    });

    test("basic functionality", () => {
        const values = [
            ["UTC", "+00:00"],
            ["GMT", "+00:00"],
            ["Etc/GMT+12", "-12:00"],
            ["Etc/GMT-12", "+12:00"],
            ["Europe/London", "+00:00"],
            ["Europe/Berlin", "+01:00"],
            ["America/New_York", "-05:00"],
            ["America/Los_Angeles", "-08:00"],
            ["+00:00", "+00:00"],
            ["+01:30", "+01:30"],
        ];
        for (const [arg, expected] of values) {
            const instant = new Temporal.Instant(1600000000000000000n);
            expect(new Temporal.TimeZone(arg).getOffsetStringFor(instant)).toBe(expected);
        }
    });
});

describe("errors", () => {
    test("this value must be a Temporal.TimeZone object", () => {
        expect(() => {
            Temporal.TimeZone.prototype.getOffsetStringFor.call("foo");
        }).toThrowWithMessage(TypeError, "Not an object of type Temporal.TimeZone");
    });
});
