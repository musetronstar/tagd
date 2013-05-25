# Rules for Defining Tags

## Determining the Super Relation

[Language is a map, but the map is not the territory.][1]

1. Tag ids refer to the thing not the label.  The label is arbitrary, but the
   thing refered to is not.  For example, consider these two statements:
       dog _is_a word
       dog _is_a animal
   They are both correct in a sense, but we are interested in the thing being
   refered to, not the label itself.  Another example:
       http://en.wikipedia.org/wiki/Dog _is_a _url
       http://en.wikipedia.org/wiki/Dog _is_a wiki
   Likewise, we are not interested in identifying what the URL is (its obvious
   by its syntax).  We are interested in the resource refered to.

[1]: http://www.slideshare.net/timoreilly/language-is-a-map-pdf-with-notes "Language is a Map"
