// Module-scoped cache for base URL
let BASE_URL = null;

// Returns the base URL (protocol + host) for this app
export function getBaseURL() {
	if (!BASE_URL) {
		const { protocol, host } = window.location;
		BASE_URL = `${protocol}//${host}`;
	}
	return BASE_URL;
}

/**
 * Sends a HTTP PUT request to the httagd server 
 * Used for TAGL PUT operations (e.g. ">> tag _sub tag").
 *
 * @param {string} tagId - The name of the tag (subject)
 * @param {string} body - TAGL statement
 * @returns {Promise<Response>} - Resolves with the fetch Response
 */
export async function putTag(tagId, body) {
	const res = await fetch(`${getBaseURL()}/${encodeURIComponent(tagId)}`, {
		method: "PUT",
		headers: { "Content-Type": "text/plain; charset=utf-8" },
		body
	});
	if (!res.ok) throw new Error(await res.text());
	return res;
}

/**
 * Sends a POST request to the httagd server to append a new predicate to a tag.
 *
 * @param {string} tagId - The name of the tag (subject)
 * @param {string} body - TAGL-formatted predicate statement
 * @returns {Promise<Response>} - Resolves with the fetch Response
 */
export async function postTag(tagId, body) {
	const res = await fetch(`${getBaseURL()}/${encodeURIComponent(tagId)}`, {
		method: "POST",
		headers: { "Content-Type": "text/plain; charset=utf-8" },
		body
	});
	if (!res.ok) throw new Error(await res.text());
	return res;
}

