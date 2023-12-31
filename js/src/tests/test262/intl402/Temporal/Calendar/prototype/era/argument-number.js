// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.calendar.prototype.era
description: A number is converted to a string, then to Temporal.PlainDate
features: [Temporal]
---*/

const instance = new Temporal.Calendar("iso8601");

const arg = 19761118;

const result = instance.era(arg);
assert.sameValue(result, undefined, "19761118 is a valid ISO string for PlainDate");

const numbers = [
  1,
  -19761118,
  1234567890,
];

for (const arg of numbers) {
  assert.throws(
    RangeError,
    () => instance.era(arg),
    `Number ${arg} does not convert to a valid ISO string for PlainDate`
  );
}

reportCompare(0, 0);
