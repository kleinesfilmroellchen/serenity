## The Fidewepre patch branch of SerenityOS

This branch of [SerenityOS](https://www.github.com/SerenityOS/serenity) hosts the **Fidewepre** application. Fidewepre is short for **Filmröllchen's De-Webified Presenter** and represents a customized version of Presenter split off of the main branch before Presenter was modified into a LibWeb-backed application. It includes a lot of additional features from my original Presenter vision as well as a few features I merged from other people's unfinished PRs.

### Why?

To explain why Fidewepre exists, we have to start at the beginning.

Presenter is an app I built for myself in late 2022 to hold presentations within Serenity, possibly about Serenity. After others expressed interest in this app (I wasn't sure if anyone except me cared about a "SerenityOS PowerPoint"), I decided to develop it cleanly as a new base system app, and it was added in PR #15718. I set up an issue tracker for features I (1) needed for usability and (2) wanted to have because they're cool. Within weeks, some other small patches were made while I prepared most of my needed features in a large PR. In fact, this very branch is the same that that PR was originally prepared on.

At the very beginning of 2023, the maintainers held a private discussion (no logs are available to this day) about the future of Presenter, and it was decided that Presenter should share more code with LibWeb because it was using its own bespoke rendering engine and could be using LibWeb instead. Once the discussion went public on [#ui](https://discord.com/channels/830522505605283862/830529037251641374/1060173916510900307),  I objected to this on several fronts, mainly on the performance and complexity basis. I will not repeat the entire argument because (1) you probably don't need to be convinced and (2) most of the maintainers will not be convinced. However, I agreed to a middle ground, where we would start a process of extracting significantly sophisticated pieces of the LibWeb rendering engine into LibGfx or a third library, including control via CSS features and possibly web-like layouting features. This was not accepted by the maintainer consensus and the decision was finalized: As soon as possible, LibWeb would be in place as the Presenter backend.

I therefore, especially after seeing the first draft of the LibWeb version of Presenter, made the decision that I could not continue working on Presenter in this state. I am severely disappointed, I would have loved to cooperate and work on a solution that looked good for everyone and continue improving Presenter, but unfortunately my concerns were ignored. While I tried to approach LibWeb from a favorable angle, I see it with all the complexities it brings, and others and I had ideas of how to improve it and Presenter at the same time through modularization. Those suggestions were dismissed and the fundamental Presenter architecture dismissed, [sometimes quite impolitely](https://discord.com/channels/830522505605283862/830529037251641374/1060175015217221652). For sake of my mental health, I feel justified in not cooperating with Presenter development for the time being.

Because I need Presenter, however, I created this branch with a split-off Fidewepre. The branch is only an addition of Fidewepre to the base system, all code modifying other parts of the system should and will be mainlined.

Fidewepre will be continually worked on as long as I need it and as long as Presenter is in a "dumb" OutOfProcessWebView-state. If either of these changes, I will drop Fidewepre without a second thought.

### Running

Build SerenityOS normally, and Fidewepre will be an additional application installed into the system.

If you run this branch, it is recommended you first disable the Presenter component from the main build, as both call themselves "Presenter" towards the user.

If required, you can try to rebase this branch on SerenityOS master. I will do this myself whenever I use Fidewepre for something important, but I do not guarantee that Fidewepre runs on latest master.
