// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.plainmonthday.prototype.with
description: >
    Calendar.mergeFields method is called with null-prototype fields objects
includes: [temporalHelpers.js]
features: [Temporal]
---*/

const calendar = TemporalHelpers.calendarCheckMergeFieldsPrototypePollution();
const instance = new Temporal.PlainMonthDay(5, 2, calendar);
instance.with({ day: 24 });
assert.sameValue(calendar.mergeFieldsCallCount, 1, "mergeFields should have been called on the calendar");

reportCompare(0, 0);
