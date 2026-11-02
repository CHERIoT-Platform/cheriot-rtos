Contributing new features
=========================

The ideal flow for contributing new features is:

1. Raise an issue with a high-level description of the problem.
2. Reach consensus that the feature is desirable.
3. Create a PR with a design document.
4. Reach consensus on the design and merge the PR.
5. Create a PR that implements the design, along with any updates to the design document that are necessary based on learnings from the implementation.
6. Merge the new feature.

For early discussion, please use the [CHERIoT public Signal chat](https://signal.group/#CjQKIElxAs3t3MUEMOEmQEuMHRK4rErUk2xVeFzjAjFXAShzEhCK9qQwEMFKGLGZnCjrQ7zm) to informally discuss ideas.
This is not necessary but can avoid work later.

Framing the problem
-------------------

During steps 1 and 2, the initial focus should be on understanding problem.
This should, ideally, be driven by a few concrete use cases.
What are the current limitations that make the use cases hard?
How do other platforms solve this?
What in their approaches works well, and what works badly?
What additional constraints come from solving this problem in a privilege-separated environment?

The goal at this stage is to make sure that there is broad consensus that this is a real problem that the RTOS (and not, for example, some downstream consumer) should be solving.

Once the problem is well understood, you can start proposing a solution.

Sketching an informal design
----------------------------

Before the end of step 2, there should be rough consensus around the shape of a possible solution.
This is not a detailed design but should capture things like the locations of trust boundaries and what the core functionality will be in each layer (if applicable).
For small features, this is sufficient to skip to step 5.
For example, if the new feature results in adding a single new function, a complete design document may be overkill and the issue is sufficient.

The goal at this stage is to ensure that there is consensus on the rough shape of a solution so that, if a concrete design or implementation is suggested, there is a path to accepting it.
Objections later on should be related to the details, not the broad structure, of the solution.

Moving to a more formal design
------------------------------

When adding a new subsystem, or an approach that touches multiple components, we need a proper design document.
This should include a summary of the problem as agreed in the issue and propose a concrete design.
The design document should link to the relevant issues, but should not require that readers follow those links to understand the problem or the solution.
Issue-tracker discussions may be long and contain digressions, the design document should summarise the *conclusions* of the discussion, not the process that arrived at this conclusion.

The design document at this stage should be raised as a PR adding a new file in [the design documents folder](../design/)

At this stage, the design should be sufficiently detailed that someone else could implement it with reference to the design document.
For example, the [wall-clock subsystem design](../design/Clocks.md) includes the C++ code for the interface to a clock source, along with a description of how clock sources are used.

The goals at this stage (by the end of step 4) are to ensure that the concrete design does not contain obvious flaws and to reduce the review burden by providing a clear specification against which the implementation can be evaluated.

Finalising the implementation
-----------------------------

The implementation may involve several rounds of PR review, but they should focus on whether this is a good implementation of the design from the previous stage.
The design itself may also evolve at this stage.
If implementation found some issue with a proposed interface, for example, the design document should be updated to reflect this.
Designs are intended to guide implementation but bugs in the design should be fixed in the design, not replicated in the implementation.
Changes to the design should be part of the same PR and the PR description and commit message should explain why the design was changed.

The goal at this stage is to have an implementation of the design, which addresses a real requirement, merged into the system.
