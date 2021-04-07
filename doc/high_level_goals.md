# High Level Goals

Minc brings significant contributions in the field of modern software architecture. The following high level goals have directed the design of this library.

## Proliferation of programming languages

An ever growing amount of outstanding programming languages have made writing quality code both easier and more complex at the same time. The question about the best language for a specific domain often has no right answer, while wrong answers can incur significant cost to development.

Minc's goal is to eventually support all major programming languages, with transparent wrappers between any two of them built in. This eliminates the need for countless wrapper libraries and answers the unanswerable question: The best programming language is a super-set of all languages. By choosing Minc as the platform for an enterprise scale software system, the architect allows engineers of different teams and backgrounds to collaborate on a shared codebase using each individual's language of choice.

## Abstraction of higher level responsibilities from the core language

Many higher level tasks are traditionally performed by the programming languages itself or by language specific tools. Examples of these tasks are software configuration and version management. Each traditional language introduces separate tool chains and best practices, contributing significantly to the time it takes a programmer to master a new language.

Minc enables the creation of software-agnostic tools that consistently manage the entire software stack.

## Decoupling of software dependencies

Before the advent of the internet, software development was a linear process. Each application started at the design phase, went through some iterations of implementation and testing and eventually ended up being frozen into a final release in the form of a set of floppy disks or a CD. The possibility to update software post-release gave rise to the vicious circle of software maintenance. Today a software that is not under constant development is considered stale and outdated. Even if one were to create a perfect piece of code without any bugs, eventually one of its dependencies will introduce breaking changes with the implementation of a critical security fix or an important new feature. This will force the previously perfect code to be adapted, potentially introducing new bugs and forcing dependent code to propagate the update. With each layer of dependencies, the problem grows exponentially. A program that cannot keep up or whose developers have moved on to other projects will be marked obsolete and replaced by newer tools engineered to relive the same challenges until it will too fall out of the never ending dependency cycle.

The dependency cycle cannot be fully broken, but it's exponential blast radius can. By introducing the ability to transparently mix different programming languages and even different versions of the same language, smaller dependency cycles can be isolated on a per-import basis. It allows using old-and-proven software libraries side-by-side with cutting-edge new packages, preserving the validity of software beyond its retirement date. By shifting development efforts from maintenance. to improvement, Minc has the capability to reshape the present circular redevelopment industry into the goal oriented innovation machinery of the future.