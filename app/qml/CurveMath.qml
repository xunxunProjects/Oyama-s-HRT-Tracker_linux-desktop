pragma Singleton
import QtQuick

QtObject {
    // Map a mouse x coordinate inside the plot rectangle to the nearest
    // sampled curve point. Returns -1 outside the plot or for unusable data.
    function pointIndexAt(points, x, left, plotWidth) {
        if (!points || points.length < 2 || plotWidth <= 0)
            return -1
        if (x < left || x > left + plotWidth)
            return -1

        const minT = Number(points[0].t)
        const maxT = Number(points[points.length - 1].t)
        if (!isFinite(minT) || !isFinite(maxT) || maxT <= minT)
            return -1

        const targetT = minT + (x - left) / plotWidth * (maxT - minT)
        let best = -1
        let bestDistance = Infinity
        for (let i = 0; i < points.length; ++i) {
            const distance = Math.abs(Number(points[i].t) - targetT)
            if (distance < bestDistance) {
                bestDistance = distance
                best = i
            }
        }
        return best
    }
}
