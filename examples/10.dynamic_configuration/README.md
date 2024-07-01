Safe Configuration Management
=============================

This example shows how dynamic configuration changes can be made to compartments using the ChERIoT features of static sealed capabilities, memory claims, a futex, and a sandbox compartment for validating untrusted data.

In this model a configuration item is a named blob of data.
There are compartments which supply configuration items (for example received by them via the network) and other compartments that need to receive specific configuration items when ever they change.
None of these compartments know about or trust each other; keeping them decoupled helps to keep the system simple, and make it easy to add new items and compartments with a minimal amount of development.

## Overview

The model is similar to a pub/sub architecture with a single retained message for each item and a security policy defined at build time through static sealed capabilities.
The configuration data is declarative so there is no need or value in maintaining a full sequence of updates.
Providing the role of the broker is a config_broker compartment, which has the unsealing key.
Aligned with the pub/sub model of publishing items and subscribing for items can happen in any sequence; a subscriber will receive any data that is already published, and any subsequent updates.
This avoids any timing issues during system startup.

By defining static sealed capabilities we can control at build time:
* Which compartments are allowed to update which items
* Which compartments are allowed to receives which items

In addition the example shows how memory claims can be used to ensure that heap allocations made in one compartment (publishers) remains available to the subscribers outside of the context of the call in which it was delivered.

And finally the example shows how a separate sandpit compartment can be used by a subscriber to safely validate new data and gain trust in its content without exposing it own stack and heap.

## Compartments in the example

### Config Source (Publisher)
The **Config Source** compartment provides the publisher of configuration data.
In a real system this would receive updates from the network, although its possible that some compartments might also need to publish internal configuration updates.
For this example it simply creates new updates to two items, "config1" and "config2" on a periodic basis (for simplicity both items share the same structure).

### Config Broker (Broker)
The **config broker** exposes two cross compartment methods, set_config() to publish and on_config() to subscribe.
Internally it maintains the most recently published value of each item and the list of subscribers.
Importantly it has no predefined knowledge of which items, publishers, or subscribers exist, and it's only level of trust is provided by the static sealed capabilities that only it can inspect.

### Compartment[1-3] (Subscribers)
These compartments provide the role of subscribers; Compartment 1 is allowed to receive "config1", Compartment 2 is allowed receive "config2", and Compartment 3 is allowed to receive both "config1" and "config2".

### Validator
This compartment provides a sandpit in which a compartment can validate a configuration update without exposing its own stack or context; It is prevented from making any heap allocations of its own.
This is consistent with the principle that incoming data should be treated as being not only unsafe in content, but potentially even unsafe to parse.
It exposes a single cross compartment method to validate the configuration struct used in the example; in practice each config item would need its own validation method.
The validation used here is very basic, and implemented in a way to deliberately expose it to bound violations.

## Threads in the example
A number of thread are used to provide the dynamic behaviour.
They are described in the context of the compartments where they start and execute their idle loops, although of course they also perform cross compartment calls.


A thread in the Config Source compartment periodically makes updates to both of the configuration items.
A separate thread in Config Source compartment occasionally generates an invalid structure for "config1" which triggers a bounds violation in the validator.

A thread in the Config Broker invokes the callbacks when a configuration item is changed.
It uses a _futex_ to sleep until updates are available.
 Decoupling the arrival of new data from its delivery to subscribers ensures that the thread which is making the updated (which may be part of a network stack) isn't blocked until all of the subscribers have processed it.
 

A thread in each of the subscribing compartments registers for configuration updates, and then sits in a loop periodically printing the configuration values the compartment has received and validated.
The main reason for having this thread is to show that the value remains available after it has been freed by the Config Source.
It also provides a convenient way to perform the initial subscription for each compartment.

## Use of Sealed Capabilities
The CONFIG_CAPABILITY in each compartment holds a list of configuration item names and boolean which defines if the compartment is allowed to be a publisher.

Publishers define their capability with:
```c++
DEFINE_CONFIG_SOURCE_CAPABILITY("config1", "config2")
```
whereas subscribers define theirs with
```c++
DEFINE_CONFIG_CAPABILITY("config1")
```
In both cases the macro can take up to 8 names, which are defines in the example to be char[16].
The difference between the macros is value they supply for the is_source element.

Both methods exposed by the Config Broker require the calling compartments capability.
The set_config() method also takes name and data values, and the Config Broker checks the name against the list in the unsealed capability along with the is_source value.
The on_config() method takes a cheri_callback parameter, which is registers against all of the names in the capability.
Although it may seem more obvious to allow a subscriber to register separate callbacks for each item (since they will have different data structures) this approach ensures that a compartment always receives all of the values it is allowed, and the name is passed to the callback to allow simple conditional logic in the callback.

Using the same pattern as the allocator the Config Broker assigns an id to each capability the first time it is presented, which is used to track the association between callback function and capability in the callback lists for each item.
If a compartment calls on_config() a second time then the previous callback is replaced.


## Memory Management
In the example the ConfigSource allocates a new heap object for each updated, and frees it immediately after the call to set_config() - which is prototypical of something working from a network stack and ensured that new values do not overwrite existing ones until they have been validated.

Subscribers can not be allowed to change the configuration data (it may be shared with other compartments) so the Config Broker cderives a Read Only capability to pass to subscribers.

The Config Broker also need to ensure that it can provide the data to any current and future subscribers, so it adds its own claim to the object, which it only releases when it gets a new object for that item.
The claim is made against the derived read only capability.

Any subscribers which need to have access the configuration data beyond the scope of the callback can also make their own claim on the object, and release it on the next update.

## Sandpit Compartment
Although the static sealed capabilities provide protection over who can update and receive configuration items, they can not offer any assurance over the content.
To treat the data as initially untrusted we have to assume that not only may it contain invalid values, but that it may be constructed so as to cause harm when it is being parsed.
In the example the first action of each callback is to pass the received value to a validation method in a sandpit container, which has no access to the callers stack or context and cannot allocate any additional heap.
An error handler in the validator compartment traps any violations, and if the validator does not return a success value the compartment simply ignores the update and keeps its claim on the previous object.

## Running the example
The example is built and run with the normal xmake commands, and has no external dependencies.

Thread priorities are uses to ensure a start up sequence that has subscribed run both before and after the initial data is available.

Debug messages from Config Source show when data is updated, and from each compartment when it receives an update plus a timer driven view of the value between updates.

Every 12 seconds a malicious value is sent for "config1" which triggers a BoundsViolation in the validator.

Here are some examples of the output

Compartment 1 registers for updates before any data is available.
```
Compartment #1: thread 0x3 Register for config updates
Config Broker : thread 0x3 on_config called for config1 by id 0x1
```

Compartment 2 registers for updates and data is already available, so the callback is immediately invoked.
All of this happens on the same thread.
```
Compartment #2: thread 0x4 Register for config updates
Config Broker : thread 0x4 on_config called for config2 by id 0x3
Compartment #2: thread 0x4 Update config2 -- config count: 0x0 token: eggman
```

Config Sources sets a value for item "config1", and Compartment 1 and Compartment 3's callbacks are called.
Note the change in thread as the callback is invoked by the thread in the Config Broker.
```
Config Source : thread 0x2 Set config1
Config Broker : thread 0x1 processing 0x1 updates
Compartment #1: thread 0x1 Update config1 -- config count: 0x0 token: walrus
Compartment #3: thread 0x1 Update config1 -- config count: 0x1 token: walrus
```

Compartment 1 prints it's current value from a timer
```
Compartment #1: thread 0x3 Timer  config1 -- config count: 0x1 token: walrus
```

Config Source sets a malicious value for "config 1".
Compartment 1 & Compartment 3 call into the validator sandpit which traps the error.
```
Config Source : thread 0x6 Sending bad data for config1
Config Broker : thread 0x1 processing 0x1 updates
Validator: Detected BoundsViolation(0x1) in validator.  Register CA0(0xa) contained invalid value: 0x8000dd58 (v:1 0x8000dd50-0x8000dd58 l:0x8 o:0x0 p: G R----- -- ---)
Compartment #1: thread 0x1 Validation failed for config1 0x8000dd50 (v:1 0x8000dd50-0x8000dd58 l:0x8 o:0x0 p: G R----- -- ---)
Validator: Detected BoundsViolation(0x1) in validator.  Register CA0(0xa) contained invalid value: 0x8000dd58 (v:1 0x8000dd50-0x8000dd58 l:0x8 o:0x0 p: G R----- -- ---)
Compartment #3: thread 0x1 Validation failed for config1 0x8000dd50 (v:1 0x8000dd50-0x8000dd58 l:0x8 o:0x0 p: G R----- -- ---)
```

## To Do

_These are more considerations for an operational system than the example_

Currently we use the same static sealing type for publish and subscribe capabilities, with a bool to define the permission.Although this is defined first in the struct and so should be easy to distinguish in the audit data, maybe separate sealing types would be even easier to audit.

If two subscribers both have access to the same item then they will both validate it; in some cases this may be OK (they may have different validation requirements), but in others it could be inefficient (although it is always safe).
It's not clear what could track the validated status; the item itself should be immutable (accessed via a read only capability), and the validator stateless.
It might be possible for the validator to call the Config Broker, which could record the status and pass it on to subsequent callbacks - but the complexity of this and the additional attack surface it creates makes me feel its not worth it, esp since shared config values are probably the exception.

