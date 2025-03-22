<p>
<h1>{{id}}</h1>
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
<!--
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
-->

<button class="tagd:open-diaglog">＋</button>
<dialog class="tagd:dialog" style="visibility:hidden" open>
<form name="tagl_form" action="javascript:void(0);" onsubmit="postTAGL()" role="add tagl predicate">
 <input id="tagl_url" type="hidden" value="{{REQUEST_URL_VIEW_TAGL}}">
 <input name="tagl" id="tagl_input" type="text" required>
 <button>add predicate</button><br>
 <code id="tagl_error">&nbsp;</code>
</form>
</dialog>
<script src="/_file/script/tagl.js"></script>
