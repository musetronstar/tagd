{{#sub_relation}}
<h4>
&rarr;
<!--<a href="{{sub_relator_lnk:h}}">{{sub_relator:h}}</a>-->
<a href="{{super_object_lnk:h}}">{{super_object:h}}</a>
</h4>
{{/sub_relation}}

{{#relations}}
 <ul>
 {{#relation}}
   <li>
    <a href="{{relator_lnk:h}}">{{relator:h}}</a>
    <a href="{{object_lnk:h}}">{{object:h}}</a>
	 {{#has_modifier}}= {{modifier:h}}{{/has_modifier}}
  </li>
 {{/relation}}
 </ul>
{{/relations}}

<!--<p style="visibility: hidden;"><em><small>{{num_results:h}}</small></em> results</p>-->

{{#results}}
<ol>
 {{#result}}
 <li>
 <h3><a href="{{res_id_lnk:h}}">{{res_id:h}}</a></h3>
 <!--&rarr;
 <a href="{{res_sub_relator_lnk:h}}">{{res_sub_relator:h}}</a>
 <a href="{{res_super_object_lnk:h}}">{{res_super_object:h}}</a>
 -->
{{#res_relations}}
 <ul>
 {{#res_relation}}
  <li>
  <small><a href="{{res_relator_lnk:h}}">{{res_relator:h}}</a></small>
  <a href="{{res_object_lnk:h}}">{{res_object:h}}</a>
  {{#res_has_modifier}} = {{res_modifier:h}}{{/res_has_modifier}}</li>
 {{/res_relation}}
 </ul>
{{/res_relations}}
 </li>
 {{/result}}
 </ol>
{{/results}}
