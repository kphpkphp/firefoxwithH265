// |reftest| skip-if(!this.hasOwnProperty('Temporal')) -- Temporal is not enabled unconditionally
// Copyright (C) 2022 Igalia, S.L. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-temporal.zoneddatetime.from
description: A calendar ID is valid input for Calendar
features: [Temporal]
---*/

const calendar = "iso8601";

const timeZone = new Temporal.TimeZone("UTC");
const arg = { year: 1970, monthCode: "M01", day: 1, timeZone, calendar };
const result = Temporal.ZonedDateTime.from(arg);
assert.sameValue(result.calendarId, "iso8601", `Calendar created from string "${calendar}"`);
assert.sameValue(result.getISOFields().calendar, "iso8601", "calendar slot stores a string");

reportCompare(0, 0);
