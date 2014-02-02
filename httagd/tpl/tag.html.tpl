<p>
<h1>{{id}}</h1>
&rarr;<a href="{{super_relator_lnk}}">{{super_relator}}</a> <a href="{{super_object_lnk}}">{{super_object}}</a>
</p>
{{#image_jpeg}}<img src="{{img_src}}">{{/image_jpeg}}
{{#relations}}
<ul>
 {{#relation}}
 <li>
  <a href="{{relator_lnk}}">{{relator}}</a>
  <a href="{{object_lnk}}">{{object}}</a>
   {{#has_modifier}} = {{modifier}}{{/has_modifier}}
 </li>
 {{/relation}}
</ul>
{{/relations}}

{{#referents_refers_to}}
<h3>Referents</h3>
 <h4>Refers To</h4>
  <ul>
   <li>
	<a href="{{refers_lnk}}">{{refers}}</a>
	<a href="{{refers_to_relator_lnk}}">{{refers_to_relator}}</a>
	<a href="{{refers_to_lnk}}">{{refers_to}}</a>
	{{#has_context}}
	<a href="{{context_relator_lnk}}">{{context_relator}}</a>
	<a href="{{context_lnk}}">{{context}}</a>
	{{/has_context}}
   </li>
 </ul>
{{/referents_refers_to}}

{{#referents_refers}}
 <h4>Refers</h4>
  <ul>
   <li>
	<a href="{{refers_lnk}}">{{refers}}</a>
	<small><a href="{{refers_to_relator_lnk}}">{{refers_to_relator}}</a></small>
	<a href="{{refers_to_lnk}}">{{refers_to}}</a>
	{{#has_context}}
	<a href="{{context_relator_lnk}}">{{context_relator}}</a>
	<a href="{{context_lnk}}">{{context}}</a>
    {{/has_context}}
   </li>
 </ul>
{{/referents_refers}}

{{#objects_related}}
<h3><a href="{{query_related_lnk}}">Related</a></h3>
<ul>
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
</ul>
{{/objects_related}}

<ul>
{{#siblings}}
 <li><a href="{{sibling_lnk}}">{{sibling}}</a></li>
{{/siblings}}
</ul>

<ul>
{{#children}}
 <li><a href="{{child_lnk}}">{{child}}</a></li>
{{/children}}
</ul>
