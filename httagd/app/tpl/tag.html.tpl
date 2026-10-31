<h1 id="tag-id">{{id:h}}</h1>
<p>
&rarr;<a href="{{sub_relator_lnk:h}}">{{sub_relator:h}}</a>
<a href="{{super_object_lnk:h}}">{{super_object:h}}</a>
</p>
{{#has_img}}<img src="{{img_src:h}}">{{/has_img}}
{{#relations}}
 {{#relation_set}}
<dl>
 <dt><a href="{{relator_lnk:h}}">{{relator:h}}</a></dt>
 {{#relation}}
 <dd>
  <a href="{{object_lnk:h}}">{{object:h}}</a>
   {{#has_modifier}} = {{modifier:h}}{{/has_modifier}}
 </dd>
 {{/relation}}
</dl>
 {{/relation_set}}
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
