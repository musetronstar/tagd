## The tagd Semantic-Relational Engine

In a hurry? Go directly to [Installation](#installation)

The purpose of the tagd semantic-relational engine is to allow
*intelligent agents* (human or machine) to assign and retrieve the semantic
meaning of tags through a meta-conceptual language called **TAGL**.

By defining *super relations*, tags can be represented as a tree of knowledge
called a **tagspace**.  TAGL maps to a tagspace.  A web service called
**httagd** in turn maps to TAGL, enabling communication over the Web and
integration with other services.

All tokens in TAGL are UTF-8 labels.

### TAGL Tutorial

This tutorial is intended to be read through, or followed by performing the
actions.

To perform the actions, first make the software (see [Installation](#installation)),
then launch **tagsh** - the tagd shell:

	cd tagsh
	bin/tagsh --db -

The "--db -" option will tell tagsh to create a sqlite in-memory database.  Otherwise,
it will open/create a default database file (~/.tagspace.db).

#### PUT Statement

A tag can be a metaphysical concept or a concrete entity.  It can be anything
you can think of and put into words.  To define a tag, you must define its
*super relation*, given by `_super`, `_is_a`, or `_type_of`:

	put dog _is_a mammal;

Whoah, we have an error:

	TAGL_ERR _type_of _error
	_has _msg = "unknown tag: mammal"

The object (animal in this case) of a super relation must also be defined. This
requires a bit of abstract metaphysical thinking, but here we go:

	put physical_object _is_a _entity;
	put living_thing _is_a physical_object;
	put animal _is_a living_thing;
	put vertibrate _is_a animal;
	put mammal _is_a vertibrate;
	put dog _is_a mammal;

Note the predicate `_is_a _entity`.  Tags with a leading underscore ('\_') are
known as *hard tags*, that is, they are hard-coded in the software, defined for
you, and cannot be redefined.  User defined tags with a leading underscore are
illegal.  The `_entity` tag is the root of the tagspace - it does not get more
abtract than that.

The `_entity` tag is axiomatic and self-referencing (i.e. `_entity _is_a _entity`).
It is the only tag having this relation.

Now let's put some more tags so that we can define dog more completely:

	put can _type_of _relator;
	put event _is_a _entity;
	put sound _is_a event;
	put utterance _is_a sound;
	put bark _is_a utterance;
	put body_part _is_a physical_object;
	put legs _is_a body_part;
	put tail _is_a body_part

	put dog
	can bark
	_has legs = 4, tail;

Note the missing semicolon (';') after the statement `put tail _is_a body_part`.
That's not a syntax error.  A statement can be terminated by a semicolon, `<EOF>`,
or a double newline.

The equals sign ('=') after "legs" indicates that the "4" is a *modifier* (more
specifically a *quantifier*), that is, it quantifies the object.  Modifiers are
optional, but if present they must be preceded by an equals sign.

The intent of a modifier is to assign values to objects when appropriate.  You
can think of objects and modifiers as key/value pairs that are attributes
of a tag.

Note, when speaking of subjects and objects, this is meant in the linguistic
sense of the terms (i.e. subject-verb-object).  Also, I tend to use the terms
relations and predicates interchangeably.

Now is a good time to define a put statement more formally...

##### PUT Grammar:

	put_statement ::= "PUT" subject_super_relation relations
	put_statement ::= "PUT" subject_super_relation 
	put_statement ::= "PUT" subject relations

	subject_super_relation ::= subject SUPER TAG

	subject ::= TAG

	relations ::= relations predicate_list
	relations ::= predicate_list

	predicate_list ::= relator object_list

	relator ::= TAG

	object_list ::= object_list ',' object
	object_list ::= object

	object ::= TAG EQUALS QUANTIFIER
	object ::= TAG EQUALS MODIFIER
	object ::= TAG

Where TAG and MODIFIER are UTF-8 labels composed of alphanumeric characters and
underscores and a QUANTIFIER is a number.  The `SUPER TAG` predicate defines
the subject as being subordinate to (i.e. "is a" or child relation) another tag.

#### GET Statement

Now we can get the tag we've defined:

	get dog;

Results should be:

	dog _is_a mammal
	can bark
	_has legs = 4, tail

##### GET Grammar:

	get_statement ::= "GET" subject

The definition will likely be expanded to allow for getting subject lists
of more than one tag in the future.

#### QUERY Statement

Tags (subjects) can be retrieved according to matching predicates using query
statements.  Let's define some more tags to make it interesting:

	put lives_in _type_of _relator;
	put substance _is_a _entity;
	put fluid _is_a substance;
	put water _is_a fluid;
	put gas _is_a substance;
	put air _is_a gas;
	put body_fluid _is_a substance;
	put blood _is_a body_fluid;
	put fins _is_a body_part;
	put gills _is_a body_part;
	put hair _is_a body_part;
	put scales _is_a body_part;
	put breathes _is_a _relator;
	put what _is_a _interrogator;

	put fish _is_a vertibrate
	_has gills, fins, scales
	lives_in water
	breathes water

	put mammal
	_has hair, blood = warm
	breathes air

	put whale _is_a mammal
	_has fins
	lives_in water;

Now let's try a query:

	query what _is_a animal
	lives_in water;

Results should be:

	whale, fish

But `whale _is_a mammal` and `fish _is_a vertibrate` - how did we query for
`_is_a animal` and get a match?  Since tagspace is organized as a tree, and
animal is a parent (aka superordinate or hypernym) of both vertibrate and
mammal, it is logically true that whale and fish match.

Let's try another:

	query what lives_in water
	breathes air
	_has fins, blood = warm;

Results should be:

	whale

###### Wildcard Relator:

	query what * water, fins, blood = warm;

Results should be:

	whale

The wildcard ('\*') means "is related to" in the most general sense.  It allows
you to match objects while leaving out relators.

##### QUERY Grammar:

	query_statement ::= "QUERY" interrogator_super_relation relations 
	query_statement ::= "QUERY" interrogator_super_relation
	query_statement ::= "QUERY" INTERROGATOR relations

	interrogator_super_relation ::= INTERROGATOR SUPER TAG

#### URLs

URLs are recognized and parsed in TAGL with no special syntax required.  Unlike
other tags, URLs cannot currently be assigned a super relation.  A super relation
`_is_a _url` is assigned for you when putting a URL.  This will likely change
in the future so that tags "referenced by url" can be defined (e.g. "wiki").

Let's put some urls:

	put about _type_of _relator

	put https://en.wikipedia.org/wiki/Dog
	about dog

	put http://animal.discovery.com/breed-selector/dog-breeds.html
	about dog

	put https://en.wikipedia.org/wiki/Whale
	about whale;

Now we can query some URLs:

	query what _is_a _url
	about dog;

Results:

	https://en.wikipedia.org/wiki/Dog,
	http://animal.discovery.com/breed-selector/dog-breeds.html
	
And some more:

	query what * _private = wikipedia, _pub = org
	about animal;

Results:

	https://en.wikipedia.org/wiki/Dog,
	https://en.wikipedia.org/wiki/Whale

Now let's get a url:

	get https://en.wikipedia.org/wiki/Dog ;

Results:

	https://en.wikipedia.org/wiki/Dog _is_a _url
	_has _host = en.wikipedia.org, _path = /wiki/Dog, _private = wikipedia, _pub = org, _scheme = https, _sub = en
	about dog

As you can see, when a URL is inserted, it is parsed and its components
are inserted according to the following hard tags:

* `_url` - super tag for URLs
* `_url_part` - the super tag of url part hard tags
	* `_host` - host of a URL
	* `_private` - private label of a registerable TLD
	  (i.e. the "wikipedia" in wikipedia.org)
	* `_pub` - label(s) of the TLD (can be a multi-level TLD).
	  Public and private (effective) TLDs are parsed according to the
	  [Mozilla Public Suffix List](http://publicsuffix.org/)
	* `_sub` - label(s) of the subdomain
	* `_path` 
	* `_query` - query string of the URL
	* `_fragment`
	* `_port`
	* `_user`
	* `_pass`
	* `_scheme`

#### Referents and Contexts

TAGL uses referents as a way of dealing with different names that refer to the
same thing (synonyms), and different things referred by the same name
(ambiguity).  The use of contexts is crucial in this regard.

#### Referent Statement:

If you would like to alias hard tags, you can do so:

	put is_a _refers_to _is_a;
	put has _refers_to _has;
	put context _refers_to _context;
	put refers _refers_to _refers;

We can even alias `_refers_to`:

	put refers_to _refers_to _refers_to;

Here is a synonym for animal:

	put creature refers_to animal;

Now, we can `get animal` by refering to creature:

	get creature;

Results:

	creature _is_a living_thing
	refers_to animal  

Since no context was specified, creature refers to animal in the *universal
context* (same as *null* context).

Here are some referents within a context:

	put plant is_a living_thing;
	put fruit is_a plant;
	put citrus is_a fruit;
	put lemon is_a citrus;

	put quality is_a _entity;
	put color is_a quality;
	put yellow is_a color;

	put machine is_a physical_object;
	put automobile is_a machine;
	put used_car is_a automobile;

	put lemon refers_to yellow context color;
	put lemon refers_to used_car context automobile;

Now, let's get lemon in the universal context:

	get lemon;

Results:

	lemon _is_a citrus;

No surprises - now let's set a context...

#### SET Statement:

	set _context automobile;
	get lemon;

Results:

	lemon _is_a automobile
	refers_to used_car

Context heirarchies can also be set, where the closest (from right to left)
context is tested for a match before moving up the context list:

	set context automobile, color;
	get lemon;

Results:

	lemon _is_a color
	refers_to yellow

Example of a Japanese referent:

	put communication is_a _entity;
	put language is_a communication;
	put japanese is_a language;
	put イヌ refers_to dog context japanese;

First, here is a gotcha:

	get イヌ;

Results:

	TS_AMBIGUOUS _type_of _error
	_has _msg = "イヌ refers to a tag with no matching context"

Though there is currently only one referent of "イヌ", its not safe to resolve
it in the universal context, as one could later define another referent of the
same label and alter program logic, as it then truly would be ambiguous.

If a referent is defined in a context, that context must be set to resolve it:

	set context japanese;
	get イヌ;

Results:

	イヌ _is_a mammal
	can bark
	has legs = 4, tail
	refers_to dog

The context can be cleared by setting it to an empty string:

	set context "";

##### QUERY Referents:

You can see what a label *refers* by using a query:

	query what _refers lemon;

Results:

	lemon _refers_to used_car
	_context automobile

	lemon _refers_to yellow
	_context color

And with a context:

You can see what a label *refers* by using a query:

	query what _refers lemon _context machine;

Results:

	lemon _refers_to used_car
	_context automobile

Likewise, you can query what *refers to* a tag:

	query what _refers_to dog;

Results:

	イヌ _refers_to dog
	_context japanese

Or, you can query all referents given a context:

	put クジラ _refers_to whale _context japanese;

	query what _context japanese;

Results:

	イヌ _refers_to dog
	_context japanese

	クジラ _refers_to whale
	_context japanese

Finally, you can query all referents:

	query what is_a _referent;

Results:

	creature _refers_to animal

	lemon _refers_to used_car
	_context automobile

	lemon _refers_to yellow
	_context color

	イヌ _refers_to dog
	_context japanese

	クジラ _refers_to whale
	_context japanese

Through the use of referents, we can state predicates in other languages, and
have those predicates immediately available in the context they refer to.

###### Spanish Language Example:

Let's set up referents that will allow us to express the definition of dog in
Spanish:

	put spanish is_a language;
	put perro refers_to dog context spanish;
	put es_un refers_to is_a context spanish;
	put mamífero refers_to mammal context spanish;
	put puede refers_to can context spanish;
	put ladrar refers_to bark context spanish;
	put tiene refers_to has context spanish;
	put patas refers_to legs context spanish;
	put cola refers_to tail context spanish;

	put fur is_a body_part;
	put piel refers_to fur context spanish;

Now let's add a predicate in Spanish:

	set context spanish;
	put perro tiene piel;

Now we can see the predicate that was stated in Spanish:

	set context "";
	get dog;

Results:

	dog is_a mammal
	can bark
	has fur, legs = 4, tail

We can see the Spanish predicate `tiene piel` is available in English as
`has fur`.

Lets dump all the data we have entered so far to file so we can reuse it:

	.dump /tmp/tutorial.tagl

To exit the shell type `.exit` or `^D`.

### tagd Web Service

Just as TAGL is a map onto a tagspace, a the tagd web service (*httagd*) maps
a *tagdurl* to TAGL.  With tagdurls, the *service is the language*.

The httagd web server is very minimal at this point - only a few features have been
implemented, but here goes...

First, fire up the webserver:

	# assuming you are still in the tagsh/ directory
	cd ../httagd
	bin/httagd --db - &

httagd should now be listening on port 8082 and using an in-memory database.

You can post valid TAGL to httagd. Lets load the data we just dumped previously:

	curl -XPOST http://localhost:8082/ --data-binary @/tmp/tutorial.tagl

Note, though the .tagl file is UTF-8 text data, the --data-binary option
prevents curl from messing with the new line characters.

Lets put some data:

	curl -XPUT http://localhost:8082/meow --data 'is_a utterance'
	curl -XPUT http://localhost:8082/cat --data 'is_a mammal can meow has legs = 4, tail'

Which executes the TAGL statements:

	PUT meow is_a utterance

	PUT cat is_a mammal
	can meow
	has legs = 4, tail

Note, the subject is in the tagdurl, while the relations are in the data body.

Now, let's get it:

	curl -XGET http://localhost:8082/cat

Results:

	cat is_a mammal
	can meow
	has legs = 4, tail

Likewise the following TAGL statement was generated:

	GET cat

Note, there is no trailing '/' after cat in the tagdurl.  A slash after the
subject indicates a query.  For example, see the difference between:

	curl -XGET http://localhost:8082/mammal

Results:

	mammal _is_a vertibrate
	breathes air
	has blood = warm, hair

and the query:

	curl -XGET http://localhost:8082/mammal/

Results:

	dog, whale, cat

The TAGL query generated was:

	QUERY _interrogator _super mammal;

Now, let's query with some relations:

	curl -XGET http://localhost:8082/animal/meow,tail

Results:

	cat

The TAGL query generated was:

	QUERY _interrogator _super animal
	* meow, tail

To perform a query not having a super relation, use a wildcard ('\*'):

	curl -XGET http://localhost:8082/*/fins,hair,air,water

Results:

	whale

The TAGL query generated was:

	QUERY _interrogator
	* fins, hair, air, water 

So far, that is all that httagd can do.  More features will be coming soon...

### Installation

So far, this has only been developed, compiled, and tested on recent versions
of Ubuntu.  I would be happy to hear of successful installation on other
platforms.

##### Dependencies

* Build Essentials 
    * GCC C++ Compiler, headers and libraries
    * Make, etc.
* Optional, to recreate gperf header of [Mozilla's Public Suffix List](http://publicsuffix.org/)
	* [gperf](http://www.gnu.org/software/gperf/) to make a hash of TLDs
	* Net::IDN::Encode CPAN module to parse the list
* [re2c](http://re2c) for the lexer/scanner
* [lemon](http://www.hwaci.com/sw/lemon/lemon.html) for the parser
* [libevhtp](https://github.com/ellzey/libevhtp) for the http server
* [CxxTest](http://cxxtest.com/) for unit tests
* [Expect](http://www.tcl.tk/man/expect5.31/expect.1.html) for testing tagsh
* [SQLite v3](http://www.sqlite.org/) libs and development headers

Building and Testing:

    make
    make tests

### License

In the pursuit of Universal love, peace, and harmony, this is free software
committed to the public domain as [CC0](http://creativecommons.org/publicdomain/zero/1.0/).
