// |reftest| async
// This file was procedurally generated from the following sources:
// - src/class-elements/private-names.case
// - src/class-elements/productions/cls-expr-after-same-line-static-async-method.template
/*---
description: private names (field definitions after a static async method in the same line)
esid: prod-FieldDefinition
features: [class-fields-private, class, class-fields-public, async-functions]
flags: [generated, async]
includes: [propertyHelper.js]
info: |
    ClassElement :
      ...
      FieldDefinition ;

    FieldDefinition :
      ClassElementName Initializer_opt

    ClassElementName :
      PrivateName

    PrivateName :
      # IdentifierName

---*/


var C = class {
  static async m() { return 42; } #x; #y;
  x() {
    this.#x = 42;
    return this.#x;
  }
  y() {
    this.#y = 43;
    return this.#y;
  }
}

var c = new C();

assert(
  !Object.prototype.hasOwnProperty.call(c, "m"),
  "m doesn't appear as an own property on the C instance"
);
assert(
  !Object.prototype.hasOwnProperty.call(C.prototype, "m"),
  "m doesn't appear as an own property on the C prototype"
);

verifyProperty(C, "m", {
  enumerable: false,
  configurable: true,
  writable: true,
}, {restore: true});

C.m().then(function(v) {
  assert.sameValue(v, 42);

  function assertions() {
    // Cover $DONE handler for async cases.
    function $DONE(error) {
      if (error) {
        throw new Test262Error('Test262:AsyncTestFailure')
      }
    }
    // Test the private fields do not appear as properties before set to value
    assert(!Object.prototype.hasOwnProperty.call(C.prototype, "#x"), "test 1");
    assert(!Object.prototype.hasOwnProperty.call(C, "#x"), "test 2");
    assert(!Object.prototype.hasOwnProperty.call(c, "#x"), "test 3");

    assert(!Object.prototype.hasOwnProperty.call(C.prototype, "#y"), "test 4");
    assert(!Object.prototype.hasOwnProperty.call(C, "#y"), "test 5");
    assert(!Object.prototype.hasOwnProperty.call(c, "#y"), "test 6");

    // Test if private fields can be sucessfully accessed and set to value
    assert.sameValue(c.x(), 42, "test 7");
    assert.sameValue(c.y(), 43, "test 8");

    // Test the private fields do not appear as properties before after set to value
    assert(!Object.prototype.hasOwnProperty.call(c, "#x"), "test 9");
    assert(!Object.prototype.hasOwnProperty.call(c, "#y"), "test 10");
  }

  return Promise.resolve(assertions());
}).then($DONE, $DONE);
