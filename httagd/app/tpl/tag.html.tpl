<h1 id="tag-id">{{id}}</h1>
<p>
&rarr;<a href="{{sub_relator_lnk}}">{{sub_relator}}</a>
<a href="{{super_object_lnk}}">{{super_object}}</a>
</p>
{{#has_img}}<img src="{{img_src}}">{{/has_img}}
{{#relations}}
 {{#relation_set}}
<dl>
 <dt><a href="{{relator_lnk}}">{{relator}}</a></dt>
 {{#relation}}
 <dd>
  <a href="{{object_lnk}}">{{object}}</a>
   {{#has_modifier}} = {{modifier}}{{/has_modifier}}
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
