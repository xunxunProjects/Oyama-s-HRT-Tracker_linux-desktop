.pragma library
// What a dose can be, as the web app's dose form offers it: the routes each
// mode logs, the compounds each route takes, and the factor from a compound's
// mass to the estradiol or testosterone in it (the model's molecular weights).

var MW = {
    E2: 272.38, EB: 376.50, EV: 356.50, EC: 396.58, EN: 384.56, EU: 440.66,
    CPA: 416.94, T: 288.42, TC: 412.60, TE: 400.59, TU: 456.70
};

// The route menu in a fixed order, the most used first. Masculinizing HRT
// has no oral, sublingual or patch routes.
function routesFor(transmasc) {
    if (transmasc)
        return ["injection", "gel"];
    return ["injection", "oral", "sublingual", "gel", "patchApply", "patchRemove"];
}

function estersFor(route, transmasc) {
    if (transmasc)
        return route === "injection" ? ["TC", "TE", "TU"] : ["T"];
    switch (route) {
    case "injection": return ["EB", "EV", "EC", "EN", "EU"];
    case "oral": return ["E2", "EV", "CPA"];
    case "sublingual": return ["E2", "EV"];
    }
    return ["E2"];
}

function isTestosterone(ester) {
    return ester === "T" || ester === "TC" || ester === "TE" || ester === "TU";
}

function isEstradiol(ester) {
    return ["E2", "EB", "EV", "EC", "EN", "EU"].indexOf(ester) >= 0;
}

// mg of estradiol (or testosterone) per mg of the logged compound.
function factor(ester) {
    if (ester === "E2" || ester === "CPA" || !MW[ester]) return 1;
    if (isTestosterone(ester)) return MW.T / MW[ester];
    return MW.E2 / MW[ester];
}

// The sublingual tiers: how long the tablet is held, and its theta.
var TIERS = [
    { value: 0, key: "quick", hold: 2 },
    { value: 1, key: "casual", hold: 5 },
    { value: 2, key: "standard", hold: 10 },
    { value: 3, key: "strict", hold: 15 }
];

function parseExtras(value) {
    if (!value) return {};
    if (typeof value === "object") return value;
    try {
        return JSON.parse(value);
    } catch (error) {
        return {};
    }
}

// The number without trailing zeros: 4, 3.056, 12.5.
function trim(mg) {
    return Number(mg).toFixed(3).replace(/\.?0+$/, "");
}
