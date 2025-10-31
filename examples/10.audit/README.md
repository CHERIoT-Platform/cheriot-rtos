Auditing compartments
=====================

This example shows a simple use of the auditing tool.
The code in this is very simple:

 - A `caesar` compartment exposes interfaces to encrypt and decrypt data using a Caesar cypher.
 - A `producer` compartment uses this to encrypt a value using a key held in a software capability.
 - An `entry` compartment receives this encrypted data and forwards it to a `consumer` compartment.
 - The `consumer` compartment receives the data and invokes the `caesar` compartment to decrypt it.

This is a fairly contrived example but it provides a simple structure for exploring [`cheriot-audit`](https://github.com/CHERIoT-Platform/cheriot-audit).
This tool applies [Rego](https://www.openpolicyagent.org/docs/latest/policy-language/) policies to CHERIoT firmware images.

Note: For all of the `cheriot-audit` examples, we'll assume that you're running a command something like this:

```sh
$ cheriot-audit --board=../../sdk/boards/sail.json --firmware-report=build/cheriot/cheriot/release/audit.json --module=caesar.rego  --query '{query}' | jq
```

You may need to specify a full path to cheriot-audit (it is in `/cheriot-tools/bin` in the dev container).
The query will be shown before the results.
Piping the output to `jq` is optional, but will give pretty-printed output for valid JSON (remove it for places where you get an error: `undefined` is not rendered by `jq`).

The `--board` argument is the same as the board JSON file that you identified with the `--board` argument you passed to `xmake` (if you didn't pass a path to that, it will look in `sdk/boards`).
The `--firmware` is the JSON file produced during the final firmware link step.
The `--module` argument is the [`caesar.rego`](caesar.rego) file that contains the policy written for this example.
Finally, the `--query` argument is the Rego query to run.

Validating our Caesar capabilities
----------------------------------

The capabilities for the Caesar cypher (defined in [`caesar_cypher.h`](caesar_cypher.h)) contain three single-byte fields:

 - A permit-encrypt permission.
 - A permit-decrypt permission.
 - A shift value (Caesar cypher is a simple substitution cypher that rotates the alphabet, and so the 'key' is the amount that each letter is shifted).

These are all static sealed objects that are held by the compartment that can authorise encryption or decryption.
To encrypt a message, you call the encrypt function exposed by the `caesar` compartment and pass it one of these capabilities.
The compartment will dynamically check that it is the right kind of capability and that it permits sealing, then encrypt with the embedded shift amount.

This has the property that any compartment that holds an authorising capability can request encryption or decryption, but does not itself know the key.

To validate the properties of our keys, let's start by defining a rule that identifies whether an import is a Caesar capability:

```rego
# Check if an import is sealed with the Caesar capability type
is_sealed_caesar_capability(capability) {
    capability.kind = "SealedObject"
    capability.sealing_type.compartment = "caesar"
    capability.sealing_type.key = "CaesarCapabilityType"
}
```

This is a unary rule (it takes one argument).
Rego rules are similar to Prolog predicates.
They are not functions that return true or false, they are logical expressions that either hold or don't.
This distinction rarely matters, but it can be confusing because a failure will be reported in Rego as `undefined` instead of `false`.

This rule holds (is true) if all of the rules listed on the lines between braces hold.
The `=` operator in Rego is *unification*, not assignment.
This means that it will try to find values on the left and right sides that allow the equality to hold.

In English, this says that the argument is a sealed Caesar capability if (and only if) its kind if `SealedObject`, the compartment that owns the sealing type is `caesar` and the sealing type is the one that this compartment exports as `CaesarCapabilityType`.

We can see how this works by using it with a Rego *comprehension*.
Try running this query:

```rego
[ c | c = input.compartments[_].imports[_] ; data.caesar.is_sealed_caesar_capability(c) ]
```

This will evaluate to an array of values of `c`, where `c` is every import from any compartment, filtered by the rule that we've just written.
The underscores are distinct anonymous variables.
Because we never constrain these, they can be any value, and so this will find any compartment, and any import in that compartment, and then filter them.

Note that we refer to our rule with a `data.caesar` prefix.
All Rego modules are imported into the `data` namespace with the module name as the second-level namespace.

The output should look something like this:

```json
[
  {
    "contents": "00015f00",
    "kind": "SealedObject",
    "sealing_type": {
      "compartment": "caesar",
      "key": "CaesarCapabilityType",
      "provided_by": "build/cheriot/cheriot/release/caesar.compartment",
      "symbol": "__export.sealing_type.caesar.CaesarCapabilityType"
    }
  },
  {
    "contents": "01005f00",
    "kind": "SealedObject",
    "sealing_type": {
      "compartment": "caesar",
      "key": "CaesarCapabilityType",
      "provided_by": "build/cheriot/cheriot/release/caesar.compartment",
      "symbol": "__export.sealing_type.caesar.CaesarCapabilityType"
    }
  }
]
```

This has found the two capabilities that we expected to find looking for.

Decoding our Caesar capabilities
--------------------------------

Seeing something like `"contents": "01005f00"` in the above example isn't that informative.
Is this a valid set of values?
The next step is to write something to decode these.

We'll start with a helper to convert integers into C booleans:

```rego
value_as_boolean(value) = output {
    value = 1
    output = true
}

value_as_boolean(value) = output {
    value = 0
    output = false
}
```

This is a Rego rule that has two definitions.
The first will hold if the value is 1, and will set the result to `true`.
The second will hold if the value is 0, and will set the result to `false`.
If the value is anything other than 0 or 1, this rule will not hold.

Try this with the following two queries:

```rego
data.caesar.value_as_boolean(1)
data.caesar.value_as_boolean(2)
```

These should give `true` and `undefined`, respectively.
Rego reports unification failure as `undefined` and this propagates upwards.
Any rule that depends on a rule that is undefined will also be undefined.
We can use this to make sure that the boolean values that we want are canonical true and false values, as well as decoding them.

With these defined, let's move on to the Rego rule that decodes one of these capabilities:

```rego
decode_user_key_capability(capability) = decoded {
    # Fail if this is not sealed with the Caesar capability type
    is_sealed_caesar_capability(capability)
    some permitEncrypt, permitDecrypt, shift

    # Extract the values.  Each of these will fail if the value is not as expected.
    # permitEncrypt is a (single-byte) boolean value at offset 0.
    permitEncrypt = value_as_boolean(integer_from_hex_string(capability.contents, 0, 1))

    # permitDecrypt is a (single-byte) boolean value at offset 1.
    permitDecrypt = value_as_boolean(integer_from_hex_string(capability.contents, 1, 1))

    # shift is a single-byte integer value at offset 2.
    shift = integer_from_hex_string(capability.contents, 2, 1)
    decoded = {
        "permitEncrypt": permitEncrypt,
        "permitDecrypt": permitDecrypt,
        "shift": shift,
    }
}
```

This starts by depending on the rule that we defined first, which will cause this to fail for anything that isn't a capability of the expected type.
Next, we define three local variables to hold the three fields that we expect.
We'll extract each of these using `integer_from_hex_string`, a built-in function provided by `cheriot-audit`.
This takes a string, an offset, and a length and will decode a little-endian integer from the hex string provided.
We're extracting three one-byte integers at offsets 0, 1, and 2.
We're then converting the first two to booleans.

Finally, if all of that worked, we're returning a new object that mirrors the C structure that represents our capability.
We can use this with another comprehension to extract and decode all valid capabilities.
This is sufficiently useful that we'll define a new rule for it:

```rego
all_valid_caesar_capabilities = [{"owner": owner, "capability": decode_user_key_capability(c)} | c = input.compartments[owner].imports[_];         is_sealed_caesar_capability(c)]
```

This is a slightly more complex comprehension.
The result is defining a new object with both the owner and the decoded capability.
Try running this:

```rego
data.caesar.all_valid_caesar_capabilities
```

You should see something like this:

```rego
[
  {
    "capability": {
      "permitDecrypt": true,
      "permitEncrypt": false,
      "shift": 95
    },
    "owner": "consumer"
  },
  {
    "capability": {
      "permitDecrypt": false,
      "permitEncrypt": true,
      "shift": 95
    },
    "owner": "producer"
  }
]
```

Check these values with the ones declared in the source code (in [`producer.cc`](producer.cc) and [`consumer.cc`](consumer.cc)).

Defining our requirements
-------------------------

Now that we have all of the helpers that let us inspect the linked image, let's define a `valid` rule that defines the policy for our linked compartment.
We'll start here by depending on the `valid` rule from the RTOS itself:

```rego
    data.rtos.valid
```

This performs some sanity checking on the RTOS core, such as ensuring that only the allocator can read hazard-pointer slots, that allocator capabilities are all valid, and so on.

Next, we'll make sure that we have the right number of Caesar capabilities and that the number of *valid* Caesar capabilities is the same:

```rego
    # There are two things sealed with the Caesar capability type
    count([c | c = input.compartments[owner].imports[_]; is_sealed_caesar_capability(c)]) = 2

    # Both of them are valid Caesar capabilities
    count(all_valid_caesar_capabilities) = 2
```

The first of these will find everything that is sealed as a Caesar capability, the second will find only ones that decode correctly.
We know that only the producer and consumer compartments should hold these capabilities, so we check that the number found is two.

Having done that, we extract the two capabilities that we expect to exist:

```rego
    some producerCapability, consumerCapability
    producerCapability = [c | c = all_valid_caesar_capabilities[_]; c.owner = "producer"][0].capability
    consumerCapability = [c | c = all_valid_caesar_capabilities[_]; c.owner = "consumer"][0].capability
```

Each of these starts with a comprehension that filters the set of all Caesar capabilities to find the ones with the named owner and then extracts the capability.
Note that, in this case, we can assume that there is a single value here and so hard code array index 0.
If that assumption is incorrect then either our previous assertion that there are two capabilities in total, or a later assertion where we inspect properties of the capabilities, will fail.

Now that we have these, ensure that the producer is permitted to encrypt and the consumer to decrypt, but not vice versa.

```rego
    producerCapability.permitEncrypt = true
    producerCapability.permitDecrypt = false
    consumerCapability.permitEncrypt = false
    consumerCapability.permitDecrypt = true
```

We expect the consumer to be able to decrypt things encrypted by the producer, so let's also make sure that their shift values are the same:

```rego
producerCapability.shift = consumerCapability.shift
```

Finally, for some defence in depth, let's make sure that the producer is the only caller of the encrypt function and the consumer the only caller of the decrypt function:

```rego
    data.compartment.compartment_call_allow_list("caesar", "caesar_encrypt.*", {"producer"})
    data.compartment.compartment_call_allow_list("caesar", "caesar_decrypt.*", {"consumer"})
```

This is not necessary in theory, because these require an authorising capability and so another compartment calling them should fail.
Another compartment trying to call them is definitely a bug though, so it's worth checking.
Similarly, let's make sure that the producer and consumer are called only from the entry compartment

```rego
    data.compartment.compartment_call_allow_list("producer", "produce_message.*", {"entry"})
    data.compartment.compartment_call_allow_list("consumer", "consume_message.*", {"entry"})
```

Putting this all together, we can now run a very simple query:

```rego
data.caesar.valid
```

If this all worked, the result should be simply `true`.

A policy like this can be included in CI to make sure that everything that you commit meets the policy.
It can drive key release for code signing, so that you never sign a firmware image that doesn't meet your policy.
Along the way to writing the final policy, we built a set of tools for introspection on the firmware image, so you can query properties.

Note on cryptography
--------------------

This example uses a Caesar Cypher.
This was chosen because it is easy to implement, not because it is secure.
Post-quantum encryption algorithms are designed to be robust against hypothetical quantum computers.
Modern classical encryption algorithms are robust against attacks by large classical computers.
The Caesar Cypher is not robust against a person with a pen and paper.

Under no circumstances should you copy the encryption portion of this code into anything that needs to be robust in the presence of an adversary with ten minutes and a piece of paper.
Breaking Caesar Cyphers is a fun thing for small children to do, not a challenge for a real cryptanalyst.

All of that said, the same mechanisms used in this example *can* be used with more sensible encryption schemes to ensure key confidentiality.
