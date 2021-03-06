# Returns the fully qualified package of a node
def packageOf(node):
	package = ''
	node = node.parent
	while node:
		if type(node) is Module:
			package = node.symbolName() + '.' + package
		node = node.parent
	return package

# Returns a list of all packages a class imports
def dependsOnPackages(aClass):
	result = []
	for decl in aClass.subDeclarations:
		if type(decl) is NameImport:
			package = ''
			name = decl.importedName
			while type(name) is ReferenceExpression:
				package = name.name + '.' + package
				name = name.prefix
			result.append(package)
	return result

allPackages = set()
efferent = {}
afferent = {}

# Loop over input classes to collect
# package dependencies
for tuple in Query.input.take('ast'):
	p = packageOf(tuple.ast)
	allPackages.add(p)
	deps = dependsOnPackages(tuple.ast)
	if deps:
		efferent[p] = 1 + (efferent[p] if p in efferent else 0)
		for dep in deps:
			afferent[dep] = 1 + (afferent[p] if p in afferent else 0)

# Compute the instability of each package
for p in allPackages:
	e = efferent[p] if p in efferent else 0
	a = afferent[p] if p in afferent else 0
	i = str( e/(e+a) ) if e+a > 0 else 1
	t = Tuple([('package', p), ('instability', i)])
	Query.result.add(t)