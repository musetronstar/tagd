/***** Unified behavior module for TAGD UI *****/

import { setupTree } from './tree.js';
import { setupTag } from './tag.js';

document.addEventListener("DOMContentLoaded", () => {
	setupTree();
	setupTag();
});
