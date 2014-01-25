{{#errors}}
<ol>
 {{#error}}
 <li>
 <h2>{{err_id}}</h2>
 &rarr;
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
 </ol>
{{/errors}}
