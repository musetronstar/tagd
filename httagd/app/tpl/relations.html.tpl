<!--p>
<h1>{{id:h}}</h1>
&rarr;<a href="{{sub_relator_lnk:h}}">{{sub_relator:h}}</a> <a href="{{super_object_lnk:h}}">{{super_object:h}}</a>
</p-->
{{#image_jpeg}}<img src="{{img_src:h}}">{{/image_jpeg}}
{{#relations}}
<ul>
 {{#relation}}
 <li>
  <a href="{{relator_lnk:h}}">{{relator:h}}</a>
  <a href="{{object_lnk:h}}">{{object:h}}</a>
   {{#has_modifier}} = {{modifier:h}}{{/has_modifier}}
 </li>
 {{/relation}}
</ul>
{{/relations}}

{{#referents_refers_to}}
<h3>Referents</h3>
 <h4>Refers To</h4>
  <ul>
   <li>
	<a href="{{refers_lnk:h}}">{{refers:h}}</a>
	<a href="{{refers_to_relator_lnk:h}}">{{refers_to_relator:h}}</a>
	<a href="{{refers_to_lnk:h}}">{{refers_to:h}}</a>
	{{#has_context}}
	<a href="{{context_relator_lnk:h}}">{{context_relator:h}}</a>
	<a href="{{context_lnk:h}}">{{context:h}}</a>
	{{/has_context}}
   </li>
 </ul>
{{/referents_refers_to}}

{{#referents_refers}}
 <h4>Refers</h4>
  <ul>
   <li>
	<a href="{{refers_lnk:h}}">{{refers:h}}</a>
	<small><a href="{{refers_to_relator_lnk:h}}">{{refers_to_relator:h}}</a></small>
	<a href="{{refers_to_lnk:h}}">{{refers_to:h}}</a>
	{{#has_context}}
	<a href="{{context_relator_lnk:h}}">{{context_relator:h}}</a>
	<a href="{{context_lnk:h}}">{{context:h}}</a>
    {{/has_context}}
   </li>
 </ul>
{{/referents_refers}}

{{#objects_related}}
<h3><a href="{{query_related_lnk:h}}">Related</a></h3>
<ul>
 <li><a href="{{related_lnk:h}}">{{related:h}}</a>
  <ul>
 {{#related_predicate}}
   <li>
    <a href="{{related_relator_lnk:h}}">{{related_relator:h}}</a>
    <a href="{{related_object_lnk:h}}">{{related_object:h}}</a>
  {{#has_related_modifier}} = {{modifier:h}}{{/has_related_modifier}}
   </li>
 {{/related_predicate}}
  </ul>
 </li>
</ul>
{{/objects_related}}

<ul>
{{#siblings}}
 <li><a href="{{sibling_lnk:h}}">{{sibling:h}}</a></li>
{{/siblings}}
</ul>

<ul>
{{#children}}
 <li><a href="{{child_lnk:h}}">{{child:h}}</a></li>
{{/children}}
</ul>
