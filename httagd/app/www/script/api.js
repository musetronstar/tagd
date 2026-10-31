// Returns the base URL (protocol + host) for this window
export function getBaseURL() {
	if (!getBaseURL.cached) {
		const loc = window.location;
		getBaseURL.cached = loc.origin || (loc.protocol + "//" + loc.host);
	}
	return getBaseURL.cached;
}

// Returns the tagdurl for a tagId
export function tagdURL(tagId) {
	// Edits belong to the tagspace selected by the page's first path segment.
	const selector = window.location.pathname.split("/")[1] || "_";
	return `${getBaseURL()}/${selector}/${encodeURIComponent(tagId)}`;
}

const HTTP_HEADERS = { "Content-Type": "text/plain; charset=utf-8" };

/**
 * Sends a HTTP PUT request to the httagd server
 * Creates or replaces a complete description (e.g. ">>= tag _sub parent;").
 *
 * @param {string} tagId - The name of the tag (subject)
 * @param {string} taglBody - One complete >>= statement
 * @returns {Promise<Response>} - Resolves with the fetch Response
 */
export async function putTag(tagId, taglBody) {
	const res = await fetch(tagdURL(tagId), {
		method: "PUT",
		headers: HTTP_HEADERS,
		body: taglBody
	});
	if (!res.ok) throw new Error(await res.text());
	return res;
}

/**
 * Sends a POST request to the httagd server to append a new predicate to a tag.
 *
 * @param {string} tagId - The name of the tag (subject)
 * @param {string} taglBody - One complete >> or !! relation statement
 * @returns {Promise<Response>} - Resolves with the fetch Response
 */
export async function postTag(tagId, taglBody) {
	const res = await fetch(tagdURL(tagId), {
		method: "POST",
		headers: HTTP_HEADERS,
		body: taglBody
	});
	if (!res.ok) throw new Error(await res.text());
	return res;
}

/**
 * Sends a POST request to the httagd server where
 * body content contains valid TAGL statements
 *
 * @param {string} taglBody - TAGL statements
 * @returns {Promise<Response>} - Resolves with the fetch Response
 */
export async function postTAGL(taglBody) {
	const res = await fetch(getBaseURL()+'/', {
		method: "POST",
		headers: HTTP_HEADERS,
		body: taglBody
	});
	if (!res.ok) throw new Error(await res.text());
	return res;
}

