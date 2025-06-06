{{>header_html_tpl}}
<aside id="sidebar">
<nav>
{{>tree_html_tpl}}
</nav>
</aside>
<article id="content">
<section id="tag_container">
{{>tag_html_tpl}}
</section>
<hr>
<section>
{{>results_html_tpl}}
</section>
</article>
{{>footer_html_tpl}}
<form id="add-child-form">
  <input type="text" id="add-child-name" placeholder="put subordinate entity" autocomplete="off" required autofocus>
  <input type="hidden" id="add-child-parent">
  <button type="submit">Add</button>
</form>