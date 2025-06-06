<!DOCTYPE HTML>
<html>
<head>
<meta charset='utf-8'>
<title>{{title}}</title>
<link rel="stylesheet" href="/_file/style/main.css">
</head>
<body>
<header>
 <span id="logo">
  <bigger><strong>tagd</strong></bigger>
 </span>
 <span id="search">
  <form id="search-form" method="GET" action="/">
{{#query_options}}
 {{#query_option}}
 <input type="hidden" name="{{query_key}}" value="{{query_value}}">
 {{/query_option}}
{{/query_options}}
   <input id="terms" name="q" type="text" required>
   <button type="submit">Search</button>
  </form>
 </span>
 <hr>
</header>
<main id="layout">
{{>main_html_tpl}}
</main>
<div id="nav-hint"></div>
<script type="module" src="/_file/script/main.js"></script>
</body>
</html>
