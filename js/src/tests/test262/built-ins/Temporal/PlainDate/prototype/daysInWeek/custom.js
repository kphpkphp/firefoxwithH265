// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-get-temporal.plaindate.prototype.daysinweek
description: Custom calendar tests for daysInWeek().
includes: [compareArray.js]
features: [Temporal]
---*/

let calls = 0;
class CustomCalendar extends Temporal.Calendar {
  constructor() {
    super("iso8601");
  }
  daysInWeek(...args) {
    ++calls;
    assert.compareArray(args, [pd], "daysInWeek arguments");
    return 7;
  }
}

const calendar = new CustomCalendar();
const pd = new Temporal.PlainDate(1830, 8, 25, calendar);
const result = pd.daysInWeek;
assert.sameValue(result, 7, "result");
assert.sameValue(calls, 1, "calls");

reportCompare(0, 0);
