
{{#super_relation}}
<h4>
&rarr;
<!--<a href="{{super_relator_lnk}}">{{super_relator}}</a>-->
<a href="{{super_object_lnk}}">{{super_object}}</a>
</h4>
{{/super_relation}}

{{#relations}}
 <ul>
 {{#relation}}
   <li>
    <a href="{{relator_lnk}}">{{relator}}</a>
    <a href="{{object_lnk}}">{{object}}</a>
	 {{#has_modifier}}= {{modifier}}{{/has_modifier}}
  </li>
 {{/relation}}
 </ul>
{{/relations}}

<!--<p style="visibility: hidden;"><em><small>{{num_results}}</small></em> results</p>-->

{{#results}}
<ol>
 {{#result}}
 <li>
 <h3><a href="{{res_id_lnk}}">{{res_id}}</a></h3>
 <!--&rarr;
 <a href="{{res_super_relator_lnk}}">{{res_super_relator}}</a>
 <a href="{{res_super_object_lnk}}">{{res_super_object}}</a>
 -->
{{#res_relations}}
 <ul>
 {{#res_relation}}
  <li>
  <small><a href="{{res_relator_lnk}}">{{res_relator}}</a></small>
  <a href="{{res_object_lnk}}">{{res_object}}</a>
  {{#res_has_modifier}} = {{res_modifier}}{{/res_has_modifier}}</li>
 {{/res_relation}}
 </ul>
{{/res_relations}}
 </li>
 {{/result}}
 </ol>
{{/results}}
