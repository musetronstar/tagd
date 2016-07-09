{{#errors}}
<ul>
 {{#error}}
 <li>
 <strong>{{err_id}}</strong>
 <a href="{{err_super_relator_lnk}}">{{err_super_relator}}</a>
 <a href="{{err_super_object_lnk}}">{{err_super_object}}</a>
{{#err_relations}}
 <ul>
 {{#err_relation}}
  <li>
  <a href="{{err_relator_lnk}}">{{err_relator}}</a>
  <a href="{{err_object_lnk}}">{{err_object}}</a>
  {{#err_has_modifier}} = {{err_modifier}}{{/err_has_modifier}}</li>
 {{/err_relation}}
 </ul>
{{/err_relations}}
 </li>
 {{/error}}
</ul>
{{/errors}}
