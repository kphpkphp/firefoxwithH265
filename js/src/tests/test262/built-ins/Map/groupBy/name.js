// |reftest| shell-option(--enable-array-grouping) skip-if(!Object.groupBy||!xulRuntime.shell) -- array-grouping is not enabled unconditionally, requires shell-options
// Copyright (c) 2023 Ecma International.  All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
esid: sec-map.groupby
description: Map.groupBy property name descriptor
info: |
  Map.groupBy ( items, callbackfn )

  ...

    17 ECMAScript Standard Built-in Objects

  ...

includes: [propertyHelper.js]
features: [array-grouping, Map]
---*/

verifyProperty(Map.groupBy, "name", {
  value: "groupBy",
  enumerable: false,
  writable: false,
  configurable: true
});

reportCompare(0, 0);
