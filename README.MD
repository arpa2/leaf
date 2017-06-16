# Leaf (LDAP Extra Attribute Filters)

> *LDAP is protocol for data communication, with rich semantics for data
> modelling, security and database administration.  The security is
> security-in-the-large, though, to protect the communications and
> the integrity of the database itself.  Security in-the-small, for
> instance dealing with the privacy of individual attributes, is not
> a concern for most LDAP servers.  Leaf adds LDAP middleware; a
> man-in-the-middle LDAP proxy server that can filter queries and
> responses to ensure the privacy or confidentiality of the attributes
> passing through the middleware.

Leaf builds on the base provided by [LillyDAP][1] and [Quick DER][2]
to provide middleware -- for instance, filters -- that can live between
an LDAP server and the clients of that server.

The Leaf middlewares are all stand-alone executables that perform one
or more tasks in processing LDAP queries; they are transparent to clients
and the server. Middleware executables can be arranged as a pipeline of
LDAP processing, much like a shell pipefile processes text.

## LDAP-MessageLog ##

This middleware executable (based on the ldap-mitm test from LillyDAP)
logs all LDAP messages that pass through it. The messages themselves
are unchanged.

## LDAP-QueryLog ##

This middleware executable logs queries (LDAP searches) that pass through
it; the search is logged in a human-readable format. The messages themselves
are unchanged.

## LDAP-PrivacyFilter ##

This middleware executable modifies the LDAP messages passing through it.
Fields (e.g. object attributes) may be designated *private*, *sensitive*,
or *plain*. By default, fields are plain. Private fields are removed from
the search results returned to a client. Sensitive fields are removed,
unless they match exactly a query value. Plain fields are passed through
unchanged.

As an example, consider an object class Person, with a Dutch social insurance
number attribute BSN. Our example object, Mr. Vincent Oorbeeld, has BSN
123443210.

 - If the field BSN is *private*, then no search query will return any
   data for field BSN.
 - If the field BSN is *sensitive*, then a search query which explicitly
   checks for `(BSN = 123443210)` will return data for field BSN if the
   field matches the queried value.

A query `(sn = Oorbeeld)` returns no BSN values if the BSN field is
*private* or *sensitive*, while `(&(sn = Oorbeeld)(BSN = 123443210))`
returns the BSN value if is is equal to the queried value.

Note that the filtering happens only on the data returned to the client,
and that this filtering may break the schema of the object data returned.

