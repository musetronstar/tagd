{{#errors}}
<ul>
 {{#error}}
 <li>
 <strong>{{err_id:h}}</strong>
 <a href="{{err_sub_relator_lnk:h}}">{{err_sub_relator:h}}</a>
 <a href="{{err_super_object_lnk:h}}">{{err_super_object:h}}</a>
{{#err_relations}}
 <ul>
 {{#err_relation}}
  <li>
  <a href="{{err_relator_lnk:h}}">{{err_relator:h}}</a>
  <a href="{{err_object_lnk:h}}">{{err_object:h}}</a>
  {{#err_has_modifier}} = {{err_modifier:h}}{{/err_has_modifier}}</li>
 {{/err_relation}}
 </ul>
{{/err_relations}}
 </li>
 {{/error}}
</ul>
{{/errors}}
