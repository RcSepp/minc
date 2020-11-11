var minc = require("minc");

var STRING_TYPE = new minc.MincObject(), META_TYPE = new minc.MincObject();

class MyString extends minc.MincObject
{
	constructor(str)
	{
		super();
		this.str = str;
	}
}

new minc.MincPackage("helloworld-node", function(pkgScope) {
	pkgScope.defineSymbol("string", META_TYPE, STRING_TYPE);
	pkgScope.defineExpr(minc.MincBlockExpr.parseCTplt("$L").get(0), function(parentBlock, literal) {
		return new minc.MincSymbol(STRING_TYPE, new MyString(literal.value.slice(1, -1)));
	}, STRING_TYPE);
	pkgScope.defineStmt(minc.MincBlockExpr.parseCTplt("print($E<string>)"), function(parentBlock, expr) {
		var message = expr.run(parentBlock).value;
		console.log(message.str + " from Node.js!");
	});
});
