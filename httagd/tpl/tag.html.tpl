<!DOCTYPE HTML>
<html>
<head>
<meta charset='utf-8'>
<title>{{id}}</title>
</head>
<body>

 <h1>{{id}}</h1>
&rarr;<a href="{{super_relator_lnk}}">{{super_relator}}</a> <a href="{{super_object_lnk}}">{{super_object}}</a>
 <ul>
{{#relations}}
 {{#relation}}
  <li>
   <a href="{{relator_lnk}}">{{relator}}</a>
   <a href="{{object_lnk}}">{{object}}</a>
   {{#has_modifier}} = {{modifier}}{{/has_modifier}}
  </li>
 {{/relation}}
{{/relations}}
 </ul>

 <h3>Referents</h3>
  <h4>Refers To</h4>
   <ul>
{{#referents_refers_to}}
    <li>
	<a href="{{refers_lnk}}">{{refers}}</a>
	<a href="{{refers_to_relator_lnk}}">{{refers_to_relator}}</a>
	<a href="{{refers_to_lnk}}">{{refers_to}}</a>
	{{#has_context}}
	<a href="{{context_relator_lnk}}">{{context_relator}}</a>
	<a href="{{context_lnk}}">{{context}}</a>
	{{/has_context}}
   </li>
{{/referents_refers_to}}
  </ul>
  <h4>Refers</h4>
   <ul>
{{#referents_refers}}
    <li>
	<a href="{{refers_lnk}}">{{refers}}</a>
	<a href="{{refers_to_relator_lnk}}">{{refers_to_relator}}</a>
	<a href="{{refers_to_lnk}}">{{refers_to}}</a>
	{{#has_context}}
	<a href="{{context_relator_lnk}}">{{context_relator}}</a>
	<a href="{{context_lnk}}">{{context}}</a>
    {{/has_context}}
   </li>
{{/referents_refers}}
  </ul>

 <h3><a href="{{query_related_lnk}}">Related</a></h3>
 <ul>
{{#objects_related}}
  <li><a href="{{related_lnk}}">{{related}}</a>
   <ul>
 {{#related_predicate}}
	 <li>
	 <a href="{{related_relator_lnk}}">{{related_relator}}</a>
	 <a href="{{related_object_lnk}}">{{related_object}}</a>
	 {{#has_related_modifier}} = {{modifier}}{{/has_related_modifier}}
	 </li>
 {{/related_predicate}}
   </ul>
  </li>
{{/objects_related}}
 </ul>

 <h3><a href="{{query_siblings_lnk}}">Siblings</a></h3>
 <ul>
{{#siblings}}
  <li><a href="{{sibling_lnk}}">{{sibling}}</a></li>
{{/siblings}}
 </ul>

 <h3><a href="{{query_children_lnk}}">Children</a></h3>
 <ul>
{{#children}}
  <li><a href="{{child_lnk}}">{{child}}</a></li>
{{/children}}
 </ul>

 <footer><pre>{{errors}}</pre></footer>
</body>
</html>

