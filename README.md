ROASORT(8) - System Manager's Manual

# NAME

**roasort** - order and deduplicate ROA ip address information

# DESCRIPTION

The
**roasort**
utility reads a list of ROA IP address information elements from
*stdin*,
sorts and deduplicates the elements according to the canonicalization process
described in
*draft-ietf-sidrops-rfc6482bis*.

# EXIT STATUS

The **roasort** utility exits&#160;0 on success, and&#160;&gt;0 if an error occurs.

# EXAMPLES

	cat << EOF | ./roasort
	10.0.0.0/24
	10.0.0.0/24-24
	10.0.0.0/8
	2001:db8:db8::/48
	2001:db8::/32
	EOF
	10.0.0.0/8
	10.0.0.0/24
	2001:db8::/32
	2001:db8:db8::/48

# STANDARDS

*A Profile for Route Origin Authorizations (ROAs)*,
[https://datatracker.ietf.org/doc/draft-ietf-sidrops-rfc6482bis/](https://datatracker.ietf.org/doc/draft-ietf-sidrops-rfc6482bis/).

# AUTHORS

Job Snijders &lt;[job@fastly.com](mailto:job@fastly.com)&gt;

OpenBSD 7.3 - July 26, 2023
