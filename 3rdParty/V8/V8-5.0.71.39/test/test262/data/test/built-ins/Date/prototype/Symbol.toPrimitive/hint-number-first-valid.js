// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.
/*---
es6id: 20.3.4.45
description: >
    Behavior when `hint` is "number" and first try returns a valid value
info: >
    1. Let O be the this value.
    2. If Type(O) is not Object, throw a TypeError exception.
    3. If hint is the String value "string" or the String value "default", then
       a. Let tryFirst be "string".
    4. Else if hint is the String value "number", then
       a. Let tryFirst be "number".
    5. Else, throw a TypeError exception.
    6. Return OrdinaryToPrimitive(O, tryFirst).
features: [Symbol.toPrimitive]
---*/

var voCallCount = 0;
var tsAccessCount = 0;
var obj = {
  valueOf: function() {
    voCallCount += 1;
    return 'valueOf test262';
  },
  get toString() {
    tsAccessCount += 1;
  }
};

assert.sameValue(
  Date.prototype[Symbol.toPrimitive].call(obj, 'number'),
  'valueOf test262'
);
assert.sameValue(voCallCount, 1, '`valueOf` method was invoked exactly once');
assert.sameValue(tsAccessCount, 0, '`toString` method was not referenced');
