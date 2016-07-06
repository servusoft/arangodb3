// Copyright (C) 2015 the V8 project authors. All rights reserved.
// This code is governed by the BSD license found in the LICENSE file.

/*---
description: >
    When a `yield` token appears within the DestructuringAssignmentTarget of an
    AssignmentRestElement and outside of a generator function body, it should
    behave as an IdentifierReference.
es6id: 12.14.5
flags: [onlyStrict]
negative: SyntaxError
---*/

var x = {};
[...x[yield]] = [];
