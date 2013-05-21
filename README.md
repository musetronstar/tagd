## The tagd Semantic-Relational Engine

In a hurry? Go directly to [Installation](#installation)

The purpose of the tagd semantic-relational engine is to allow
*intelligent agents* (human or machine) to assign and retrieve the
semantic meaning of tags through an abstract meta-conceptual language.

By defining *identity relations*, tags can be represented as a tree -
a tree of knowledge, called a **tagspace**.  An abstract meta-conceptual
language, called **TAGL** will then map to the tree.  A REST web service will
in turn map to TAGL to enable communication over the Web and
integration with other services.

### Tags and Relations - a Tree of Knowledge

A tag can be a metaphysical concept or a concrete entity.  It can be anything
you can think of and put into words.  An **abstract_tag** object, or **tag**
for brevity, is a primitive unit that can have semantic meaning assigned.

#### Identification is Primary

A thing must be identified before it can be communicated.  The identity of a
tag is known by the combination of its **id** and its **super** relation.  A
tag id is simply a UTF-8 label, while the super relation refers to it parent
node in the tree (also known as hyperonomy or a superordinate relation).

A tag can only have one super relation.  Examples of the most common super
relations would be an **_is_a**, or **_type_of** relation.

(Note, tags that begin with a leading '_' are known as *hard tags*, because
they are hard coded.  Definition of tags with a leading '_' is illegal for
users).

From the super relation, a tree of tags is constructed having a special tag
in the root called **_entity**.  The \_entity tag has a circular relation with
itself (e.g. \_entity is a \_entity_).  The _entity tag is the only tag allowed
to follow this premise - it is axiomatic.

#### To Be, is to Be Related

A tag can also be related to other tags through the definition of zero or more
attributes.  A common attribute for things is the **_has_a** attribute.

URIs can also be represented as tags in the system so that they can be related
to other tags and URIs alike.

#### Ideas are Networks

Once a tag is put in proper relation to other tags, it is no longer merely a
label.  It is a node in a *network of conceptual integrations*.  From this,
it our hope that ideas can be communicated more concisely, with less ambiguity,
because they are referred to within their proper **context**.  This integrated
tag tree is called a **tagspace**.

#### A Rose is a Rose is a Rose

Just as A == A, tag == tag

Tag ids are global, that is, a tag id refers to one and only one canonical tag.

But because a label can refer to different things in different contexts, TAGL
also provide a means to define **referents**, so that an otherwise ambiguous
label can refer to a canonical tag based on its context, and thus more easily
disambiguated.  

### TAGL - a Meta-Conceptual Language (not yet implemented)

A meta-conceptual language, called TAGL, will be used to define and query tags,
relations and contexts.

TAGL is a formal language, but it will hopefully also be intuitive so that it
may be accessible to a wide variety of intelligent agents.

#### Language is a Map

TAGL maps tags to a tagspace.  Every label is a tag in the TAGL language.  
Tags are are primitives.  Statements take on the form of subject/predicate
pairs.

These statements themselves (if they exist) are subtrees of a tagspace.
Therefore the tagspace is defined and queried by mapping statements to it.

##### Universal Meta-Conceptual Language

A context can include details such as time, location and language.  The
use of contexts and referents should allow TAGL to represent concepts
independently of the natural language the are defined or queried in.

This should not be confused with translation, rather it enables separate
*linguistic views* to map to the same underlying structure.

### tagd REST Web Service (not yet implemented)

Just as TAGL is a map onto a tagspace, a URL in the tagd web service (*httagd*)
can be represented as TAGL.  A URL in such a representation is known as a
*tagdurl*.

### Installation

So far, this has only been developed, compiled, and tested on recent versions
of Ubuntu.  We would be very happy to hear of successful installation on other
platforms.

Dependencies:
* Build Essentials 
    * GCC C++ Compiler, headers and libraries
    * Make, etc.
* gperf (only to recreate gperf header from Mozilla's public suffix list)
    * Net::IDN::Encode CPAN module
* [CxxTest](http://cxxtest.com/)
* [SQLite v3](http://www.sqlite.org/) libs and development headers

Building and Testing:

    make
    make tests

### License

In the pursuit of Universal love, peace, and harmony, this software is
committed to the public domain.

For lawyers and pedants, this software is specifically licensed as
[CC0](http://creativecommons.org/publicdomain/zero/1.0/).
