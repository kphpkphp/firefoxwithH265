// |reftest| shell-option(--enable-array-grouping) skip-if(!Object.groupBy||!xulRuntime.shell) -- array-grouping is not enabled unconditionally, requires shell-options
// Copyright (c) 2023 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-object.groupby
description: Callback is not called and object is not populated if the iterable is empty
info: |
  Object.groupBy ( items, callbackfn )

  ...
  GroupBy ( items, callbackfn, coercion )

  6. Repeat,
    c. If next is false, then
      i. Return groups.
  ...
features: [array-grouping]
---*/

const original = [];

const obj = Object.groupBy(original, function () {
  throw new Test262Error('callback function should not be called')
});

assert.notSameValue(original, obj, 'Object.groupBy returns an object');
assert.sameValue(Object.keys(obj).length, 0);

reportCompare(0, 0);
