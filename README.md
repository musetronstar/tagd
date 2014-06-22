## The tagd Semantic-Relational Engine

<img src="http://tagd.ws/tagd-cloud.png" align="right" width="300px" />

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

	>> dog _is_a mammal;

Whoah, we have an error:

	TAGL_ERR _type_of _error
	_caused_by _unknown_tag = mammal

The object (mammal in this case) of a super relation must also be defined. This
requires a bit of abstract metaphysical thinking, but here we go:

	>> physical_object _is_a _entity;
	>> living_thing _is_a physical_object;
	>> animal _is_a living_thing;
	>> vertibrate _is_a animal;
	>> mammal _is_a vertibrate;
	>> dog _is_a mammal;

The `>>` is the input operator for putting data.  Note the predicate
`_is_a _entity`.  Tags with a leading underscore ('\_') are known as
*hard tags*, that is, they are hard-coded in the software, defined for you, and
cannot be redefined.  User defined tags with a leading underscore are illegal. 
The `_entity` tag is the root of the tagspace - it does not get more
abtract than that.

The `_entity` tag is axiomatic and self-referencing (i.e. `_entity _is_a _entity`).
It is the only tag having this relation.

Now let's put some more tags so that we can define dog more completely:

	>> can _type_of _relator;
	>> event _is_a _entity;
	>> sound _is_a event;
	>> utterance _is_a sound;
	>> bark _is_a utterance;
	>> body_part _is_a physical_object;
	>> legs _is_a body_part;
	>> tail _is_a body_part

	>> dog
	can bark
	_has legs = 4, tail;

Note the missing semicolon (';') after the statement `>> tail _is_a body_part`.
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

Now is a good time to define a input (`>>`) statement more formally...

##### PUT Grammar:

	put_statement ::= ">>" subject_super_relation relations
	put_statement ::= ">>" subject_super_relation 
	put_statement ::= ">>" subject relations

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
	object ::= TAG EQUALS QUOTED_STR
	object ::= TAG

Where TAG and MODIFIER are UTF-8 labels composed of alphanumeric characters and
underscores and a QUANTIFIER is a number. A modifier can hold spaces and other
special characters if it is enclosed in double quotes.  Double quotes as part
of the modifier value must be escaped (i.e. `"my \"quoted\" modifier"`).
The `SUPER TAG` predicate defines the subject as being subordinate to
(i.e. "is a" or child relation) another tag.

##### Comments:

Comments can be used in TAGL like this:

	-- this is a comment until the end of line
	-* this is a block comment *-	

#### GET Statement

Now we can get the tag we've defined:

	<< dog;

Results should be:

	dog _is_a mammal
	can bark
	_has legs = 4, tail

##### GET Grammar:

	get_statement ::= "<<" subject

As you can see, the `<<` input operator is used for getting data.
The definition will likely be expanded to allow for getting subject lists
of more than one tag in the future.

#### DELETE Statement

Let's set up a nonsense tag and delete it:

	>> fangs _is_a body_part;

	>> turtle _is_a mammal
	_has legs, fangs;

To delete a particular relation, use the `!!` operator:

	!! turtle _has fangs;

Now the tag will still exist, but the relation will have been deleted:

	<< turtle;

Results should be:

	turtle _is_a mammal
	_has legs

But a turtle is not a mammal, so let's delete it:

	!! turtle;

And it should no longer exist:

	<< turtle;

	TS_NOT_FOUND _type_of _error
	_caused_by _unknown_tag = turtle

##### DELETE Grammar:

A DELETE statement has the same grammar as a PUT statement, except that it
cannot have a super relation.

#### QUERY Statement

Tags (subjects) can be retrieved according to matching predicates using query
statements.  Let's define some more tags to make it interesting:

	>> lives_in _type_of _relator;
	>> substance _is_a _entity;
	>> fluid _is_a substance;
	>> water _is_a fluid;
	>> gas _is_a substance;
	>> air _is_a gas;
	>> body_fluid _is_a substance;
	>> blood _is_a body_fluid;
	>> fins _is_a body_part;
	>> gills _is_a body_part;
	>> hair _is_a body_part;
	>> scales _is_a body_part;
	>> breathes _is_a _relator;
	>> what _is_a _interrogator;

	>> fish _is_a vertibrate
	_has gills, fins, scales
	lives_in water
	breathes water

	>> mammal
	_has hair, blood = warm
	breathes air

	>> whale _is_a mammal
	_has fins
	lives_in water;

Now let's try a query:

	?? what _is_a animal
	lives_in water;

Results should be:

	whale ,
	fish

But `whale _is_a mammal` and `fish _is_a vertibrate` - how did we query for
`_is_a animal` and get a match?  Since tagspace is organized as a tree, and
animal is a parent (aka superordinate or hypernym) of both vertibrate and
mammal, it is logically true that whale and fish match.

Let's try another:

	?? what lives_in water
	breathes air
	_has fins, blood = warm;

Results should be:

	whale

###### Wildcard Relator:

	?? what * water, fins, blood = warm;

Results should be:

	whale

The wildcard ('\*') means "is related to" in the most general sense.  It allows
you to match objects while leaving out relators.

##### Full Text Search

The simplest full text query is the query operator followed by a quoted string:

	?? "has fins"

Results should be:

	whale ,
	fish

You can add search terms to an interrogator to combine with other predicates:

	?? _what
	_has blood = warm ,
	    _terms = "has fins";

Results:

	whale

##### QUERY Grammar:

	query_statement ::= "??" interrogator_super_relation relations 
	query_statement ::= "??" interrogator_super_relation
	query_statement ::= "??" INTERROGATOR query_relations
	query_statement ::= "??" "<search terms>"

	query_relations ::= relations
	query_relations ::= relator HARD_TAG_TERMS = "<search terms>"

	interrogator_super_relation ::= INTERROGATOR SUPER TAG

#### URLs

URLs are recognized and parsed in TAGL with no special syntax required.  Unlike
other tags, URLs cannot currently be assigned a super relation.  A super relation
`_is_a _url` is assigned for you when putting a URL.  This will likely change
in the future so that tags "referenced by url" can be defined (e.g. "wiki").

Let's put some urls:

	>> about _type_of _relator

	>> https://en.wikipedia.org/wiki/Dog
	about dog

	>> http://animal.discovery.com/breed-selector/dog-breeds.html
	about dog

	>> https://en.wikipedia.org/wiki/Whale
	about whale;

Now we can query some URLs:

	?? what _is_a _url
	about dog;

Results:

	https://en.wikipedia.org/wiki/Dog,
	http://animal.discovery.com/breed-selector/dog-breeds.html
	
And some more:

	?? what * _private = wikipedia,
	_public = org
	about animal;

Results:

	https://en.wikipedia.org/wiki/Dog,
	https://en.wikipedia.org/wiki/Whale

Now let's get a url:

	<< https://en.wikipedia.org/wiki/Dog ;

Results:

	https://en.wikipedia.org/wiki/Dog _is_a _url
	_has _host = en.wikipedia.org, _path = /wiki/Dog, _private = wikipedia, _public = org, _scheme = https, _subdomain = en
	about dog

As you can see, when a URL is inserted, it is parsed and its components
are inserted according to the following hard tags:

* `_url` - super tag for URLs
* `_url_part` - the super tag of url part hard tags
	* `_host` - host of a URL
	* `_private` - private label of a registerable TLD
	  (i.e. the "wikipedia" in wikipedia.org)
	* `_public` - label(s) of the TLD (can be a multi-level TLD).
	  Public and private (effective) TLDs are parsed according to the
	  [Mozilla Public Suffix List](http://publicsuffix.org/)
	* `_subdomain` - subdomain label(s) 
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

	>> is_a _refers_to _is_a;
	>> has _refers_to _has;
	>> context _refers_to _context;
	>> refers _refers_to _refers;

We can even alias `_refers_to`:

	>> refers_to _refers_to _refers_to;

Here is a synonym for animal:

	>> creature refers_to animal;

Now, we can `<< animal` by refering to creature:

	<< creature;

Results:

	creature _is_a living_thing
	refers_to animal  

Since no context was specified, creature refers to animal in the *universal
context* (same as *null* context).

Here are some referents within a context:

	>> plant is_a living_thing;
	>> fruit is_a plant;
	>> citrus is_a fruit;
	>> lemon is_a citrus;

	>> quality is_a _entity;
	>> color is_a quality;
	>> yellow is_a color;

	>> machine is_a physical_object;
	>> automobile is_a machine;
	>> used_car is_a automobile;

	>> lemon refers_to yellow context color;
	>> lemon refers_to used_car context automobile;

Now, let's get lemon in the universal context:

	<< lemon;

Results:

	lemon _is_a citrus;

No surprises - now let's set a context...

#### SET Statement:

	%% _context automobile;
	<< lemon;

Results:

	lemon _is_a automobile
	refers_to used_car

Context heirarchies can also be set, where the closest (from right to left)
context is tested for a match before moving up the context list:

	%% context automobile, color;
	<< lemon;

Results:

	lemon _is_a color
	refers_to yellow

Example of a Japanese referent:

	>> communication is_a _entity;
	>> language is_a communication;
	>> japanese is_a language;
	>> イヌ refers_to dog context japanese;

First, here is a gotcha:

	<< イヌ;

Results:

	TS_AMBIGUOUS _type_of _error
	_has _message = "イヌ refers to a tag with no matching context"

Though there is currently only one referent of "イヌ", its not safe to resolve
it in the universal context, as one could later define another referent of the
same label and alter program logic, as it then truly would be ambiguous.

If a referent is defined in a context, that context must be set to resolve it:

	%% context japanese;
	<< イヌ;

Results:

	イヌ _is_a mammal
	can bark
	has legs = 4, tail
	refers_to dog

The context can be cleared by setting it to an empty string:

	%% context "";

##### QUERY Referents:

You can see what a label *refers* by using a query:

	?? what _refers lemon;

Results:

	lemon _refers_to used_car
	_context automobile

	lemon _refers_to yellow
	_context color

And with a context:

You can see what a label *refers* by using a query:

	?? what _refers lemon _context machine;

Results:

	lemon _refers_to used_car
	_context automobile

Likewise, you can query what *refers to* a tag:

	?? what _refers_to dog;

Results:

	イヌ _refers_to dog
	_context japanese

Or, you can query all referents given a context:

	>> クジラ _refers_to whale _context japanese;

	?? what _context japanese;

Results:

	イヌ _refers_to dog
	_context japanese

	クジラ _refers_to whale
	_context japanese

Finally, you can query all referents:

	?? what is_a _referent;

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

	>> spanish is_a language;
	>> perro refers_to dog context spanish;
	>> es_un refers_to is_a context spanish;
	>> mamífero refers_to mammal context spanish;
	>> puede refers_to can context spanish;
	>> ladrar refers_to bark context spanish;
	>> tiene refers_to has context spanish;
	>> patas refers_to legs context spanish;
	>> cola refers_to tail context spanish;

	>> fur is_a body_part;
	>> piel refers_to fur context spanish;

Now let's add a predicate in Spanish:

	%% context spanish;
	>> perro tiene piel;

Now we can see the predicate that was stated in Spanish:

	%% context "";
	<< dog;

Results:

	dog is_a mammal
	can bark
	has fur, legs = 4, tail

We can see the Spanish predicate `tiene piel` is available in English as
`has fur`.

Let's dump all the data we have entered so far to file so we can reuse it:

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

httagd should now be listening on port 2112 and using an in-memory database.

You can post valid TAGL to httagd. Let's load the data we just dumped previously:

	curl -XPOST http://localhost:2112/ --data-binary @/tmp/tutorial.tagl

Note, though the .tagl file is UTF-8 text data, the --data-binary option
prevents curl from messing with the new line characters.

Let's put some data:

	curl -XPUT http://localhost:2112/meow --data 'is_a utterance'
	curl -XPUT http://localhost:2112/cat --data 'is_a mammal can meow has legs = 4, tail'

Which executes the TAGL statements:

	>> meow is_a utterance

	>> cat is_a mammal
	can meow
	has legs = 4, tail

Note, the subject is in the tagdurl, while the relations are in the data body.

Now, let's get it:

	curl -XGET http://localhost:2112/cat

Results:

	cat is_a mammal
	can meow
	has legs = 4, tail

Likewise the following TAGL statement was generated:

	<< cat

Note, there is no trailing '/' after cat in the tagdurl.  A slash after the
subject indicates a query.  For example, see the difference between:

	curl -XGET http://localhost:2112/mammal

Results:

	mammal _is_a vertibrate
	breathes air
	has blood = warm, hair

and the query:

	curl -XGET http://localhost:2112/mammal/

Results:

	dog, whale, cat

The TAGL query generated was:

	?? _interrogator _super mammal;

Now, let's query with some relations:

	curl -XGET http://localhost:2112/animal/meow,tail

Results:

	cat

The TAGL query generated was:

	?? _interrogator _super animal
	* meow, tail

To perform a query not having a super relation, use a wildcard ('\*'):

	curl -XGET http://localhost:2112/*/fins,hair,air,water

Results:

	whale

The TAGL query generated was:

	?? _interrogator
	* fins, hair, air, water 

To add search terms to your query, add a `q=<terms>` parameter to the query string:

	curl -XGET http://localhost:2112/mammal/?q=can+bark

Results:

		dog

The TAGL query generated was:

	?? _interrogator _super mammal
	_has _terms = "can bark"

To browse your tagspace, fire up your favorite browser and go to `http://localhost:2112/mammal?t=html`

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
* For testing
	* [CxxTest](http://cxxtest.com/) for unit tests
	* [Expect](http://www.tcl.tk/man/expect5.31/expect.1.html) for testing tagsh and httagd
	* [Curl](http://curl.haxx.se/) for testing httagd
* [SQLite v3](http://www.sqlite.org/) libs and development headers
* For the httagd http server
	* [libevhtp](https://github.com/ellzey/libevhtp)
	* [ctemplate](https://code.google.com/p/ctemplate/) for html/json templates

Building and Testing:

    make
    make tests

### License

In the pursuit of Universal love, peace, and harmony, this is free software
committed to the public domain as [CC0](http://creativecommons.org/publicdomain/zero/1.0/).
